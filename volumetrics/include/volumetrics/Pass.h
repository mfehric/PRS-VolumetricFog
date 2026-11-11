#pragma once

#include "volumetrics/Lighting.h"
#include "volumetrics/GraphicsServices.h"
#include <cstdint>
#include <memory>


struct VolumetricStats
{
    int width = 0, height = 0;
    float readbackMs = 0.0f, cpuIntegrationMs = 0.0f, uploadMs = 0.0f;
    float totalCpuMs = 0.0f; // CPU wall time; GPU submission is asynchronous
    bool active = false;
};

// OpenGL resources/readback/compositing live here, separate from the CPU kernel.
class VOLUMETRIC_FOG_API VolumetricPass
{
public:
    // Requires a current host GL context; loadProc initializes this DLL's GLAD.
    using LoadProc = void* (*)(const char*);
    VolumetricPass(VolumetricGraphicsServices& graphics, LoadProc loadProc,
                   const char* shaderDirectory);
    ~VolumetricPass();
    VolumetricPass(const VolumetricPass&) = delete;
    VolumetricPass& operator=(const VolumetricPass&) = delete;
    bool Render(const VolumetricFrame& frame, const VolumetricSettings& settings,
                uint32_t sceneFbo, int sceneWidth, int sceneHeight, uint32_t shadowTexture);
    const VolumetricStats& GetStats() const { return m_stats; }
    // Used by the integration validation; RGBA = (in-scattered radiance, T).
    uint32_t GetResultTexture() const { return m_fogTexture; }

private:
    bool Resize(int width, int height);
    void DestroyTargets();
    uint32_t m_depthFbo = 0, m_depthTexture = 0, m_fogFbo = 0, m_fogTexture = 0;
    int m_width = 0, m_height = 0;
    std::unique_ptr<VolumetricShader> m_gpuShader, m_compositeShader;
    VolumetricGraphicsServices& m_graphics;
    std::vector<float> m_depth, m_shadowDepth;
    std::vector<glm::vec4> m_result;
    VolumetricStats m_stats;
};
