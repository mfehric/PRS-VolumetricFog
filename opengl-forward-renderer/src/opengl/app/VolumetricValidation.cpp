#include "app/Application.h"
#include "core/Renderer.h"
#include "core/InputManager.h"
#include "core/VolumetricRay.h"
#include "scene/Scene.h"
#include "scene/FrameSubmission.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace
{
    void Require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
        spdlog::info("[VolumetricValidation] PASS: {}", message);
    }

    void CheckCPUPhysics()
    {
        VolumetricFrame frame;
        const glm::mat4 projection = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f);
        frame.inverseViewProjection = glm::inverse(projection);
        frame.lightRadiance = glm::vec3(3.0f);
        glm::vec4 projected = projection * glm::vec4(0,0,-10,1);
        const std::array<float,1> depth{projected.z / projected.w * 0.5f + 0.5f};
        std::array<glm::vec4,1> output;
        VolumetricSettings fog;
        fog.anisotropy = 0.0f; fog.density = 0.1f; fog.ambientIntensity = 0.0f;
        auto integrate = [&] { VolumetricCPU::Integrate(frame, fog, 1, 1, depth, {}, output); };
        integrate();
        const float expectedT = std::exp(-1.0f);
        const float expectedL = 3.0f * fog.scatteringAlbedo / (4.0f * glm::pi<float>()) * (1.0f - expectedT);
        Require(std::abs(output[0].w - expectedT) < 2e-5f && std::abs(output[0].x - expectedL) < 2e-5f,
                "homogeneous unoccluded ray agrees with analytic Beer-Lambert and single scattering");
        fog.density = 0.0f; integrate();
        Require(output[0] == glm::vec4(0,0,0,1), "zero density is identity");
        fog.density = 0.1f; fog.scatteringAlbedo = 0.0f; integrate();
        Require(output[0].x == 0 && std::abs(output[0].w - expectedT) < 2e-5f, "pure absorption adds no light");
        fog.density = 1e-8f; fog.scatteringAlbedo = 0.9f; integrate();
        Require(std::isfinite(output[0].x) && output[0].x > 0 && output[0].w > 0.999f, "near-zero extinction stays finite");
        fog.density = 10.0f; integrate();
        Require(std::isfinite(output[0].x) && output[0].w < 1e-20f, "dense fog tends to zero transmittance");
        frame.lightRadiance = glm::vec3(0.0f);
        fog.density = 0.1f; fog.scatteringAlbedo = 1.0f; fog.ambientIntensity = 0.35f;
        integrate();
        const auto originalFog = MarchVolumetricRay(10.0f, 0.1f, 0.35f, fog.steps);
        Require(std::abs(output[0].w - originalFog.transmittance) < 2e-5f &&
                glm::length(glm::vec3(output[0]) - glm::vec3(originalFog.inScattering)) < 2e-5f,
                "constant ambient source preserves original sequential soft fog model");
    }

    std::vector<glm::vec4> ReadTexture(uint32_t texture, int width, int height)
    {
        std::vector<glm::vec4> pixels(size_t(width) * height);
        GLint active = 0, bound = 0;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        glActiveTexture(GL_TEXTURE0); glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texture);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, pixels.data());
        glBindTexture(GL_TEXTURE_2D, bound); glActiveTexture(active);
        return pixels;
    }

    void SaveImage(const std::filesystem::path& file, const std::vector<glm::vec4>& hdr, int width, int height)
    {
        std::vector<unsigned char> rgb(size_t(width) * height * 3);
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                for (int c = 0; c < 3; ++c)
                {
                    const float v = std::max(hdr[size_t(height-1-y) * width+x][c], 0.0f);
                    const float aces = std::clamp((v*(2.51f*v+0.03f))/(v*(2.43f*v+0.59f)+0.14f), 0.0f, 1.0f);
                    rgb[(size_t(y)*width+x)*3+c] = static_cast<unsigned char>(std::pow(aces, 1.0f/2.2f)*255.0f+0.5f);
                }
        if (!stbi_write_png(file.string().c_str(), width, height, 3, rgb.data(), width*3))
            throw std::runtime_error("Could not write validation image");
    }

    double MeanScattering(const std::vector<glm::vec4>& data)
    {
        double sum = 0;
        for (auto v : data) sum += v.x + v.y + v.z;
        return sum / (data.size()*3);
    }

    void Compare(const std::vector<glm::vec4>& a, const std::vector<glm::vec4>& b)
    {
        Require(a.size() == b.size(), "CPU/GPU dimensions match");
        double squares = 0, maxError = 0;
        for (size_t i = 0; i < a.size(); ++i)
            for (int c = 0; c < 4; ++c)
            {
                if (!std::isfinite(a[i][c]) || !std::isfinite(b[i][c]))
                    throw std::runtime_error("Non-finite CPU/GPU component");
                const double error = std::abs(double(a[i][c]) - b[i][c]);
                squares += error*error; maxError = std::max(maxError, error);
            }
        const double rmse = std::sqrt(squares / (a.size()*4));
        spdlog::info("[VolumetricValidation] CPU/GPU RMSE={}, max absolute error={}", rmse, maxError);
        // Hard shadow boundaries can choose adjacent texels after floating-point rounding.
        Require(rmse < 0.001 && maxError < 0.04, "CPU/GPU agreement including hard shadow boundary tolerance");
    }
}

bool Application::ValidateVolumetrics(Scene& scene, const std::string& outputDirectory)
{
    try
    {
        CheckCPUPhysics();
        glfwHideWindow(m_window);
        constexpr int width = 640, height = 480;
        std::filesystem::create_directories(outputDirectory);
        scene.InternalUpdate(0.0f, *m_input, width, height);
        FrameSubmission sub;
        scene.BuildSubmission(sub);
        sub.clearInfo.viewport = {0,0,width,height};
        VolumetricSettings fog;
        fog.width = 240;
        auto render = [&]
        {
            m_renderer->BeginFrame(sub);
            for (auto& item : sub.objects) m_renderer->SubmitDraw(item);
            m_renderer->EndFrame(); m_renderer->RebindHdrFramebuffer(); scene.OnPostRender();
            const auto start = std::chrono::steady_clock::now();
            m_renderer->RenderVolumetrics(sub, fog);
            glFinish();
            const float completeMs = std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-start).count();
            Require(glGetError() == GL_NO_ERROR, "render pass produces no OpenGL errors");
            const auto& stats = m_renderer->GetVolumetricStats();
            spdlog::info("[VolumetricValidation] {}: pass+completion={}ms, CPU kernel={}ms, readback={}ms, upload={}ms",
                         !fog.enabled ? "Normal" : fog.backend == VolumetricBackend::CPU ? "CPU" : "GPU",
                         completeMs, stats.cpuIntegrationMs, stats.readbackMs, stats.uploadMs);
            return ReadTexture(m_renderer->GetDebugStats().hdrColorTextureId, sub.clearInfo.viewport.width, sub.clearInfo.viewport.height);
        };
        auto normal = render();
        SaveImage(std::filesystem::path(outputDirectory)/"normal.png", normal, width, height);
        fog.enabled = true;
        auto cpuImage = render();
        auto stats = m_renderer->GetVolumetricStats();
        Require(stats.active, "CPU volumetric pass is active");
        auto cpu = ReadTexture(m_renderer->GetVolumetricResultTexture(), stats.width, stats.height);
        SaveImage(std::filesystem::path(outputDirectory)/"cpu.png", cpuImage, width, height);
        fog.backend = VolumetricBackend::GPU;
        auto gpuImage = render();
        auto gpu = ReadTexture(m_renderer->GetVolumetricResultTexture(), stats.width, stats.height);
        SaveImage(std::filesystem::path(outputDirectory)/"gpu.png", gpuImage, width, height);
        Compare(cpu, gpu);
        Require(MeanScattering(cpu) > 0.001, "scene contains measurable in-scattered light");
        fog.shadows = false;
        render();
        auto unshadowed = ReadTexture(m_renderer->GetVolumetricResultTexture(), stats.width, stats.height);
        Require(MeanScattering(unshadowed) > MeanScattering(cpu) + 0.001, "occluders suppress scattered light");
        fog.shadows = true;
        fog.density = 0;
        for (auto backend : {VolumetricBackend::CPU, VolumetricBackend::GPU})
        {
            fog.backend = backend;
            auto image = render();
            float error = 0;
            for (size_t i=0; i<normal.size(); ++i)
                for (int c=0; c<3; ++c) error = std::max(error, std::abs(normal[i][c]-image[i][c]));
            Require(error < 1e-5f, "zero-density composite matches Normal Lighting");
        }
        fog.density = 0.065f;
        sub.lights.ClearDirectionalLight();
        render();
        auto noLight = ReadTexture(m_renderer->GetVolumetricResultTexture(), stats.width, stats.height);
        for (auto value : noLight)
            if (glm::length(glm::vec3(value) - fog.fogColor * (fog.ambientIntensity * fog.scatteringAlbedo * (1.0f-value.w))) > 2e-6f)
                throw std::runtime_error("Stale directional illumination without a light");
        Require(true, "removed light leaves only ambient fog, without stale directional illumination");
        scene.BuildSubmission(sub); sub.clearInfo.viewport = {0,0,800,450};
        scene.GetCamera().SetAspectRatio(800.0f/450.0f);
        fog.width = 128; fog.steps = 16;
        for (auto backend : {VolumetricBackend::CPU, VolumetricBackend::GPU})
        {
            fog.backend = backend; render();
            Require(m_renderer->GetVolumetricStats().width == 128 && m_renderer->GetVolumetricStats().height == 72,
                    "resizing and quality changes recreate valid effect targets");
        }
        sub.clearInfo.viewport = {0,0,width,height}; scene.GetCamera().SetAspectRatio(float(width)/height);
        fog.enabled = false;
        auto normalAgain = render();
        Require(normal == normalAgain, "switching back restores original Normal Lighting exactly");
        spdlog::info("[VolumetricValidation] ALL CHECKS PASSED; images in {}", outputDirectory);
        return true;
    }
    catch (const std::exception& error)
    {
        spdlog::error("[VolumetricValidation] FAILED: {}", error.what());
        return false;
    }
}
