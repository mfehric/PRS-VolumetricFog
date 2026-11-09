#pragma once

#include <array>
#include <span>
#include <vector>
#include <glm/glm.hpp>

enum class VolumetricBackend { CPU, GPU };

struct VolumetricSettings
{
    bool enabled = false;
    VolumetricBackend backend = VolumetricBackend::CPU;
    float density = 0.04f;        // sigma_t = sigma_a + sigma_s, in inverse world units
    float scatteringAlbedo = 0.9f; // sigma_s / sigma_t
    float anisotropy = 0.2f;
    float maxDistance = 50.0f;
    // Constant, unshadowed environmental source retains the original soft fog.
    // The directional source below is evaluated separately with geometry shadows.
    glm::vec3 fogColor{1.0f};
    float ambientIntensity = 0.12f;
    int width = 240;
    int steps = 64;
    bool shadows = true;
    float shadowBias = 0.0005f;
};

// Plain data shared by the sequential integrator and the OpenGL pass.
// Depth images use OpenGL's bottom-left origin and [0,1] depth convention.
struct VolumetricFrame
{
    glm::mat4 inverseViewProjection{1.0f};
    glm::mat4 view{1.0f};
    glm::vec3 cameraPosition{0.0f};
    glm::vec3 lightDirection{0.0f, -1.0f, 0.0f}; // photon travel direction
    glm::vec3 lightRadiance{0.0f};
    std::array<glm::mat4, 4> shadowMatrices{};
    std::array<float, 4> shadowSplits{};
    int shadowWidth = 0;
    int shadowHeight = 0;
    bool hasShadows = false;
};

namespace VolumetricCPU
{
    // Independent pixels/rays: the outer loop here is the future OpenMP boundary.
    // No GL calls, jobs, threads, OpenMP or explicit SIMD occur in this kernel.
    void Integrate(const VolumetricFrame& frame, const VolumetricSettings& settings,
                   int width, int height, std::span<const float> sceneDepth,
                   std::span<const float> shadowDepth, std::span<glm::vec4> output);
}
