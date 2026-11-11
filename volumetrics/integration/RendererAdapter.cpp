#include "core/Renderer.h"
#include "core/Camera.h"
#include "core/FullscreenQuad.h"
#include "core/ShaderProgram.h"
#include "scene/FrameSubmission.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <filesystem>

namespace
{
    class HostShader final : public VolumetricShader
    {
        ShaderProgram m_shader;
    public:
        HostShader(const char* vertex, const char* fragment) : m_shader(vertex, fragment) {}
        bool IsValid() const override { return m_shader.IsValid(); }
        void Bind() const override { m_shader.Bind(); }
        void SetUniform(const std::string& n, int v) const override { m_shader.SetUniform(n, v); }
        void SetUniform(const std::string& n, bool v) const override { m_shader.SetUniform(n, v); }
        void SetUniform(const std::string& n, float v) const override { m_shader.SetUniform(n, v); }
        void SetUniform(const std::string& n, const glm::vec3& v) const override { m_shader.SetUniform(n, v); }
        void SetUniform(const std::string& n, const glm::mat4& v) const override { m_shader.SetUniform(n, v); }
    };

    class HostGraphics final : public VolumetricGraphicsServices
    {
        FullscreenQuad m_quad;
    public:
        VolumetricShader* CreateShader(const char* v, const char* f) override { return new HostShader(v, f); }
        void DrawFullscreen() const override { m_quad.Draw(); }
    };

    std::unique_ptr<VolumetricGraphicsServices> CreateVolumetricGraphicsServices()
    {
        return std::make_unique<HostGraphics>();
    }
}

// Kept next to the adapter rather than mixed into the host renderer source.
void Renderer::RenderVolumetrics(const FrameSubmission& submission, const VolumetricSettings& settings)
{
    m_volumetricStats = {};
    m_debugStats.volumetric = {};
    if (!settings.enabled || !submission.camera || !m_hdrFramebuffer || !m_hdrFramebuffer->IsValid())
        return;
    VolumetricFrame frame;
    frame.inverseViewProjection = glm::inverse(submission.camera->GetViewProjection());
    frame.view = submission.camera->GetView();
    frame.cameraPosition = submission.camera->GetPosition();
    if (submission.lights.HasDirectionalLight())
    {
        const auto& light = submission.lights.GetDirectionalLight();
        if (glm::dot(light.direction, light.direction) > 1e-8f)
            frame.lightDirection = glm::normalize(light.direction);
        if (light.enabled)
            frame.lightRadiance = glm::max(light.color, glm::vec3(0.0f)) * std::max(light.intensity, 0.0f);
    }
    frame.hasShadows = m_debugStats.shadowPassDataAvailable && m_directionalShadowMap && m_directionalShadowMap->IsValid();
    frame.shadowMatrices = m_cascadeViewProj;
    frame.shadowSplits = m_cascadeSplits;
    const uint32_t shadowTexture = frame.hasShadows ? m_directionalShadowMap->GetDepthTextureArray() : 0;
    if (frame.hasShadows)
    {
        frame.shadowWidth = m_directionalShadowMap->GetWidth();
        frame.shadowHeight = m_directionalShadowMap->GetHeight();
    }
    if (!m_volumetricPass)
    {
        m_volumetricGraphics = CreateVolumetricGraphicsServices();
        // Preserve launches from the renderer source directory during development.
        // Deployed builds use the resources copied beside the executable.
        const char* shaders = VOLUMETRICS_SHADER_DIRECTORY;
        if (!std::filesystem::is_regular_file(std::filesystem::path(shaders) / "volumetric.frag"))
            shaders = VOLUMETRICS_SOURCE_SHADERS;
        m_volumetricPass = std::make_unique<VolumetricPass>(*m_volumetricGraphics,
            reinterpret_cast<VolumetricPass::LoadProc>(glfwGetProcAddress),
            shaders);
    }
    m_volumetricPass->Render(frame, settings, m_hdrFramebuffer->GetFramebuffer(),
                            m_hdrFramebuffer->GetWidth(), m_hdrFramebuffer->GetHeight(), shadowTexture);
    m_volumetricStats = m_volumetricPass->GetStats();
    m_debugStats.volumetric = m_volumetricStats;
}


