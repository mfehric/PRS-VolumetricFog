#include "core/VolumetricLighting.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <glm/gtc/constants.hpp>

namespace
{
    float Visibility(const VolumetricFrame& frame, const VolumetricSettings& settings,
                     const glm::vec3& position, std::span<const float> depth)
    {
        if (!settings.shadows || !frame.hasShadows)
            return 1.0f;
        const float viewDepth = -(frame.view * glm::vec4(position, 1.0f)).z;
        int cascade = 0;
        while (cascade < 4 && viewDepth > frame.shadowSplits[cascade]) ++cascade;
        if (cascade == 4) return 1.0f;
        const glm::vec4 clip = frame.shadowMatrices[cascade] * glm::vec4(position, 1.0f);
        if (std::abs(clip.w) < 1e-8f) return 1.0f;
        const glm::vec3 uvz = glm::vec3(clip) / clip.w * 0.5f + 0.5f;
        if (uvz.x < 0.0f || uvz.x >= 1.0f || uvz.y < 0.0f || uvz.y >= 1.0f ||
            uvz.z < 0.0f || uvz.z > 1.0f) return 1.0f;
        const int x = std::min(int(uvz.x * frame.shadowWidth), frame.shadowWidth - 1);
        const int y = std::min(int(uvz.y * frame.shadowHeight), frame.shadowHeight - 1);
        const size_t index = (size_t(cascade) * frame.shadowHeight + y) * frame.shadowWidth + x;
        return uvz.z - settings.shadowBias <= depth[index] ? 1.0f : 0.0f;
    }
}

void VolumetricCPU::Integrate(const VolumetricFrame& frame, const VolumetricSettings& settings,
                             int width, int height, std::span<const float> sceneDepth,
                             std::span<const float> shadowDepth, std::span<glm::vec4> output)
{
    const size_t count = size_t(std::max(width, 0)) * std::max(height, 0);
    if (sceneDepth.size() != count || output.size() != count ||
        (settings.shadows && frame.hasShadows &&
         (frame.shadowWidth <= 0 || frame.shadowHeight <= 0 ||
          shadowDepth.size() != size_t(frame.shadowWidth) * frame.shadowHeight * 4)))
        throw std::invalid_argument("Invalid volumetric image dimensions");

    const float density = std::max(settings.density, 0.0f);
    const float albedo = std::clamp(settings.scatteringAlbedo, 0.0f, 1.0f);
    const float g = std::clamp(settings.anisotropy, -0.9f, 0.9f);
    const int steps = std::clamp(settings.steps, 1, 256);
    const glm::vec3 lightDirection = glm::normalize(frame.lightDirection);
    const glm::vec3 ambientRadiance = glm::max(settings.fogColor, glm::vec3(0.0f)) * std::max(settings.ambientIntensity, 0.0f);

    // SEQUENTIAL PIXEL LOOP: each iteration reads immutable frame/depth data and
    // writes one output texel. Later parallelize this loop, not the depth march.
    for (size_t pixel = 0; pixel < count; ++pixel)
    {
        output[pixel] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        if (density == 0.0f) continue;
        const int x = int(pixel % width), y = int(pixel / width);
        const glm::vec2 uv((float(x) + 0.5f) / width, (float(y) + 0.5f) / height);
        const glm::vec4 clip(uv * 2.0f - 1.0f, sceneDepth[pixel] * 2.0f - 1.0f, 1.0f);
        const glm::vec4 world = frame.inverseViewProjection * clip;
        const glm::vec3 delta = glm::vec3(world) / world.w - frame.cameraPosition;
        const float surfaceDistance = glm::length(delta);
        if (surfaceDistance < 1e-6f) continue;
        const glm::vec3 ray = delta / surfaceDistance;
        const float distance = std::min(surfaceDistance, std::max(settings.maxDistance, 0.0f));
        const float stepLength = distance / steps;
        const float tau = density * stepLength;
        const float stepT = std::exp(-tau);
        // Stable 1-exp(-tau), also used verbatim in GLSL for CPU/GPU agreement.
        const float removed = tau < 0.001f ? tau * (1.0f - 0.5f * tau + tau * tau / 6.0f) : 1.0f - stepT;
        const float cosine = glm::dot(lightDirection, -ray);
        const float denominator = 1.0f + g * g - 2.0f * g * cosine;
        const float phase = (1.0f - g * g) / (4.0f * glm::pi<float>() * denominator * std::sqrt(denominator));
        const glm::vec3 source = frame.lightRadiance * (phase * albedo * removed);
        glm::vec3 scattered(0.0f);
        float transmittance = 1.0f;
        for (int sample = 0; sample < steps; ++sample)
        {
            const glm::vec3 position = frame.cameraPosition + ray * ((sample + 0.5f) * stepLength);
            scattered += transmittance * Visibility(frame, settings, position, shadowDepth) * source;
            transmittance *= stepT;
        }
        const float totalT = std::exp(-density * distance);
        scattered += ambientRadiance * (albedo * (1.0f - totalT));
        output[pixel] = glm::vec4(scattered, totalT);
    }
}
