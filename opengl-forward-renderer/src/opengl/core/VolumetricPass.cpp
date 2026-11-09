#include "core/VolumetricPass.h"
#include "core/ShaderProgram.h"
#include "core/FullscreenQuad.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <spdlog/spdlog.h>

namespace
{
    using Clock = std::chrono::steady_clock;
    float Milliseconds(Clock::time_point start)
    {
        return std::chrono::duration<float, std::milli>(Clock::now() - start).count();
    }

    // Preserve the actual GL state so Renderer's cached SubmissionContext stays valid.
    struct SavedState
    {
        GLint readFbo, drawFbo, viewport[4], program, vao, activeTexture;
        GLint texture0, texture1, array1, packBuffer, unpackBuffer, packAlignment, unpackAlignment;
        GLint srcRgb, dstRgb, srcAlpha, dstAlpha, eqRgb, eqAlpha, polygon[2];
        GLboolean depthMask;
        bool depth, blend, cull, scissor;
        SavedState()
        {
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFbo);
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);
            glGetIntegerv(GL_VIEWPORT, viewport);
            glGetIntegerv(GL_CURRENT_PROGRAM, &program);
            glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
            glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
            glActiveTexture(GL_TEXTURE0); glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture0);
            glActiveTexture(GL_TEXTURE1); glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture1);
            glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &array1);
            glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &packBuffer);
            glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpackBuffer);
            glGetIntegerv(GL_PACK_ALIGNMENT, &packAlignment);
            glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpackAlignment);
            glGetIntegerv(GL_BLEND_SRC_RGB, &srcRgb); glGetIntegerv(GL_BLEND_DST_RGB, &dstRgb);
            glGetIntegerv(GL_BLEND_SRC_ALPHA, &srcAlpha); glGetIntegerv(GL_BLEND_DST_ALPHA, &dstAlpha);
            glGetIntegerv(GL_BLEND_EQUATION_RGB, &eqRgb); glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &eqAlpha);
            glGetIntegerv(GL_POLYGON_MODE, polygon);
            glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
            depth = glIsEnabled(GL_DEPTH_TEST); blend = glIsEnabled(GL_BLEND);
            cull = glIsEnabled(GL_CULL_FACE); scissor = glIsEnabled(GL_SCISSOR_TEST);
        }
        ~SavedState()
        {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, readFbo); glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFbo);
            glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
            glUseProgram(program); glBindVertexArray(vao);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texture0);
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, texture1);
            glBindTexture(GL_TEXTURE_2D_ARRAY, array1); glActiveTexture(activeTexture);
            glBindBuffer(GL_PIXEL_PACK_BUFFER, packBuffer); glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpackBuffer);
            glPixelStorei(GL_PACK_ALIGNMENT, packAlignment); glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlignment);
            glBlendFuncSeparate(srcRgb, dstRgb, srcAlpha, dstAlpha);
            glBlendEquationSeparate(eqRgb, eqAlpha);
            glPolygonMode(GL_FRONT_AND_BACK, polygon[0]); glDepthMask(depthMask);
            depth ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
            blend ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
            cull ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);
            scissor ? glEnable(GL_SCISSOR_TEST) : glDisable(GL_SCISSOR_TEST);
        }
    };
}

VolumetricPass::VolumetricPass()
{
    m_gpuShader = std::make_unique<ShaderProgram>("assets/shaders/tone_mapping.vert", "assets/shaders/volumetric.frag");
    m_compositeShader = std::make_unique<ShaderProgram>("assets/shaders/tone_mapping.vert", "assets/shaders/volumetric_composite.frag");
    m_quad = std::make_unique<FullscreenQuad>();
}

VolumetricPass::~VolumetricPass() { DestroyTargets(); }

void VolumetricPass::DestroyTargets()
{
    glDeleteFramebuffers(1, &m_depthFbo); glDeleteFramebuffers(1, &m_fogFbo);
    glDeleteTextures(1, &m_depthTexture); glDeleteTextures(1, &m_fogTexture);
    m_depthFbo = m_fogFbo = m_depthTexture = m_fogTexture = 0;
    m_width = m_height = 0;
}

bool VolumetricPass::Resize(int width, int height)
{
    if (width == m_width && height == m_height) return true;
    DestroyTargets();
    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &m_depthTexture);
    glBindTexture(GL_TEXTURE_2D, m_depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &m_depthFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_depthFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, m_depthTexture, 0);
    glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE);
    const bool depthValid = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

    glGenTextures(1, &m_fogTexture);
    glBindTexture(GL_TEXTURE_2D, m_fogTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &m_fogFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fogFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_fogTexture, 0);
    const bool fogValid = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    if (!depthValid || !fogValid)
    {
        spdlog::error("[Volumetric] Incomplete framebuffer");
        DestroyTargets();
        return false;
    }
    m_width = width; m_height = height;
    m_depth.resize(size_t(width) * height);
    m_result.resize(size_t(width) * height);
    return true;
}

bool VolumetricPass::Render(const VolumetricFrame& frame, const VolumetricSettings& input,
                            uint32_t sceneFbo, int sceneWidth, int sceneHeight, uint32_t shadowTexture)
{
    m_stats = {};
    if (!input.enabled || sceneWidth <= 0 || sceneHeight <= 0 || !m_gpuShader->IsValid() || !m_compositeShader->IsValid())
        return false;
    const auto totalStart = Clock::now();
    SavedState saved;
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0); glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 4); glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glDisable(GL_SCISSOR_TEST);
    const int width = std::min(sceneWidth, std::clamp(input.width, 32, 640));
    const int height = std::max(1, int(std::lround(double(width) * sceneHeight / sceneWidth)));
    if (!Resize(width, height)) return false;
    VolumetricSettings settings = input;
    settings.density = std::max(settings.density, 0.0f);
    settings.scatteringAlbedo = std::clamp(settings.scatteringAlbedo, 0.0f, 1.0f);
    settings.anisotropy = std::clamp(settings.anisotropy, -0.9f, 0.9f);
    settings.steps = std::clamp(settings.steps, 1, 256);
    settings.maxDistance = std::max(settings.maxDistance, 0.0f);

    const auto readStart = Clock::now();
    glBindFramebuffer(GL_READ_FRAMEBUFFER, sceneFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_depthFbo);
    glBlitFramebuffer(0, 0, sceneWidth, sceneHeight, 0, 0, width, height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    if (settings.backend == VolumetricBackend::CPU)
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_depthFbo);
        glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, m_depth.data());
        if (settings.shadows && frame.hasShadows)
        {
            m_shadowDepth.resize(size_t(frame.shadowWidth) * frame.shadowHeight * 4);
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D_ARRAY, shadowTexture);
            glGetTexImage(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT, GL_FLOAT, m_shadowDepth.data());
        }
        m_stats.readbackMs = Milliseconds(readStart);
        const auto integrationStart = Clock::now();
        VolumetricCPU::Integrate(frame, settings, width, height, m_depth, m_shadowDepth, m_result);
        m_stats.cpuIntegrationMs = Milliseconds(integrationStart);
        const auto uploadStart = Clock::now();
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_fogTexture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_FLOAT, m_result.data());
        m_stats.uploadMs = Milliseconds(uploadStart);
    }

    glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND); glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    if (settings.backend == VolumetricBackend::GPU)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_fogFbo); glViewport(0, 0, width, height);
        m_gpuShader->Bind();
        m_gpuShader->SetUniform("u_Depth", 0); m_gpuShader->SetUniform("u_ShadowDepth", 1);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_depthTexture);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D_ARRAY, shadowTexture);
        m_gpuShader->SetUniform("u_InverseViewProjection", frame.inverseViewProjection);
        m_gpuShader->SetUniform("u_View", frame.view);
        m_gpuShader->SetUniform("u_CameraPosition", frame.cameraPosition);
        m_gpuShader->SetUniform("u_LightDirection", frame.lightDirection);
        m_gpuShader->SetUniform("u_LightRadiance", frame.lightRadiance);
        m_gpuShader->SetUniform("u_AmbientRadiance", glm::max(settings.fogColor, glm::vec3(0.0f)) * std::max(settings.ambientIntensity, 0.0f));
        m_gpuShader->SetUniform("u_HasShadows", settings.shadows && frame.hasShadows);
        m_gpuShader->SetUniform("u_ShadowBias", settings.shadowBias);
        for (int i = 0; i < 4; ++i)
        {
            m_gpuShader->SetUniform("u_ShadowMatrices[" + std::to_string(i) + "]", frame.shadowMatrices[i]);
            m_gpuShader->SetUniform("u_ShadowSplits[" + std::to_string(i) + "]", frame.shadowSplits[i]);
        }
        m_gpuShader->SetUniform("u_Density", settings.density);
        m_gpuShader->SetUniform("u_Albedo", settings.scatteringAlbedo);
        m_gpuShader->SetUniform("u_Anisotropy", settings.anisotropy);
        m_gpuShader->SetUniform("u_MaxDistance", settings.maxDistance);
        m_gpuShader->SetUniform("u_Steps", settings.steps);
        m_quad->Draw();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo); glViewport(0, 0, sceneWidth, sceneHeight);
    glEnable(GL_BLEND); glBlendEquation(GL_FUNC_ADD);
    glBlendFuncSeparate(GL_ONE, GL_SRC_ALPHA, GL_ZERO, GL_ONE);
    m_compositeShader->Bind(); m_compositeShader->SetUniform("u_Fog", 0);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_fogTexture);
    m_quad->Draw();
    m_stats.width = width; m_stats.height = height; m_stats.active = true;
    m_stats.totalCpuMs = Milliseconds(totalStart);
    return true;
}
