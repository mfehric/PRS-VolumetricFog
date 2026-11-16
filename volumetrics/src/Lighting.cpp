#include "volumetrics/Lighting.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <glm/gtc/constants.hpp>

namespace
{
    enum class ShadowMode { None, Affine, General };

    struct Cascade
    {
        // xyz columns retain GLM's original pairwise multiplication order.
        std::array<glm::vec3, 4> columns;
        const glm::mat4* matrix = nullptr;
        const float* depth = nullptr;
    };

    struct Shadows
    {
        std::array<Cascade, 4> cascades;
        std::array<float, 4> splits;
        glm::vec4 viewZ;
        int width, maxX, maxY;
        float textureWidth, textureHeight, bias;
    };

    Shadows PrepareShadows(const VolumetricFrame& frame, float bias,
                           std::span<const float> depth)
    {
        Shadows result;
        result.splits = frame.shadowSplits;
        result.viewZ = {frame.view[0][2], frame.view[1][2], frame.view[2][2], frame.view[3][2]};
        result.width = frame.shadowWidth;
        result.maxX = frame.shadowWidth - 1;
        result.maxY = frame.shadowHeight - 1;
        result.textureWidth = static_cast<float>(frame.shadowWidth);
        result.textureHeight = static_cast<float>(frame.shadowHeight);
        result.bias = bias;
        const size_t layerSize = size_t(frame.shadowWidth) * frame.shadowHeight;
        for (size_t i = 0; i < result.cascades.size(); ++i)
        {
            auto& cascade = result.cascades[i];
            cascade.matrix = &frame.shadowMatrices[i];
            for (size_t column = 0; column < 4; ++column)
                cascade.columns[column] = glm::vec3(frame.shadowMatrices[i][column]);
            cascade.depth = depth.data() + i * layerSize;
        }
        return result;
    }

    bool HasAffineOrderedShadows(const VolumetricFrame& frame)
    {
        if (!std::is_sorted(frame.shadowSplits.begin(), frame.shadowSplits.end()))
            return false;
        for (const auto& matrix : frame.shadowMatrices)
            if (matrix[0][3] != 0.0f || matrix[1][3] != 0.0f ||
                matrix[2][3] != 0.0f || matrix[3][3] != 1.0f)
                return false;
        return true;
    }

    template<ShadowMode mode>
    #if defined(_MSC_VER)
    __forceinline
    #endif
    float Visibility(const Shadows& shadows, const glm::vec3& position, int& cascade)
    {
        const auto& row = shadows.viewZ;
        const float viewDepth = -((row.x * position.x + row.y * position.y)
                               + (row.z * position.z + row.w));
        if constexpr (mode == ShadowMode::Affine)
        {
            // Reuse the previous cascade along this ray. Both directions are
            // supported, including a return from beyond the last cascade.
            while (cascade > 0 && viewDepth <= shadows.splits[cascade - 1]) --cascade;
        }
        else cascade = 0; // General path also preserves unsorted-split behavior.
        while (cascade < 4 && viewDepth > shadows.splits[cascade]) ++cascade;
        if (cascade == 4) return 1.0f;

        const auto& layer = shadows.cascades[cascade];
        glm::vec3 uvz;
        if constexpr (mode == ShadowMode::Affine)
        {
            // Directional CSM matrices have exactly w=1: no w calculation,
            // near-zero check or perspective division is needed. Do not
            // reassociate into origin+t*direction: texel-edge rounding matters.
            const auto& c = layer.columns;
            const glm::vec3 clip = (c[0] * position.x + c[1] * position.y)
                                 + (c[2] * position.z + c[3]);
            uvz = clip * 0.5f + 0.5f;
        }
        else
        {
            const glm::vec4 clip = *layer.matrix * glm::vec4(position, 1.0f);
            if (std::abs(clip.w) < 1e-8f) return 1.0f;
            uvz = glm::vec3(clip) / clip.w * 0.5f + 0.5f;
        }
        if (uvz.x < 0.0f || uvz.x >= 1.0f || uvz.y < 0.0f || uvz.y >= 1.0f ||
            uvz.z < 0.0f || uvz.z > 1.0f) return 1.0f;
        const int x = std::min(int(uvz.x * shadows.textureWidth), shadows.maxX);
        const int y = std::min(int(uvz.y * shadows.textureHeight), shadows.maxY);
        const size_t index = size_t(y) * shadows.width + x;
        return uvz.z - shadows.bias <= layer.depth[index] ? 1.0f : 0.0f;
    }

    template<ShadowMode mode>
    void IntegratePixels(const VolumetricFrame& frame, const VolumetricSettings& settings,
                         int width, int height, std::span<const float> sceneDepth,
                         std::span<glm::vec4> output, const Shadows& shadows)
    {
        const float density = std::max(settings.density, 0.0f);
        const float albedo = std::clamp(settings.scatteringAlbedo, 0.0f, 1.0f);
        const float g = std::clamp(settings.anisotropy, -0.9f, 0.9f);
        const float gSquared = g * g;
        const float twiceG = 2.0f * g;
        const int steps = std::clamp(settings.steps, 1, 256);
        const float maxDistance = std::max(settings.maxDistance, 0.0f);
        const bool directional = albedo != 0.0f && frame.lightRadiance != glm::vec3(0.0f);
        const glm::vec3 lightDirection = directional ? glm::normalize(frame.lightDirection) : glm::vec3(0.0f);
        const glm::vec3 ambientRadiance = glm::max(settings.fogColor, glm::vec3(0.0f))
                                       * std::max(settings.ambientIntensity, 0.0f);

        // SEQUENTIAL PIXEL LOOPS: row-major traversal avoids per-pixel integer
        // division/modulo; no threads, OpenMP or explicit SIMD are used.
        size_t pixel = 0;
        for (int y = 0; y < height; ++y)
        {
            const float v = (float(y) + 0.5f) / height;
            for (int x = 0; x < width; ++x, ++pixel)
            {
                output[pixel] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
                const glm::vec2 uv((float(x) + 0.5f) / width, v);
                const glm::vec4 clip(uv * 2.0f - 1.0f, sceneDepth[pixel] * 2.0f - 1.0f, 1.0f);
                const glm::vec4 world = frame.inverseViewProjection * clip;
                const glm::vec3 delta = glm::vec3(world) / world.w - frame.cameraPosition;
                const float surfaceDistance = glm::length(delta);
                if (surfaceDistance < 1e-6f) continue;
                const float distance = std::min(surfaceDistance, maxDistance);
                const float totalT = std::exp(-density * distance);
                glm::vec3 scattered(0.0f);
                if (directional)
                {
                    const glm::vec3 ray = delta / surfaceDistance;
                    const float stepLength = distance / steps;
                    const float tau = density * stepLength;
                    const float stepT = std::exp(-tau);
                    // Same stable 1-exp(-tau) and HG model as V0/the GPU shader.
                    const float removed = tau < 0.001f ? tau * (1.0f - 0.5f * tau + tau * tau / 6.0f) : 1.0f - stepT;
                    const float cosine = glm::dot(lightDirection, -ray);
                    const float denominator = 1.0f + gSquared - twiceG * cosine;
                    const float phase = (1.0f - gSquared) / (4.0f * glm::pi<float>() * denominator * std::sqrt(denominator));
                    const glm::vec3 source = frame.lightRadiance * (phase * albedo * removed);
                    float transmittance = 1.0f;
                    int cascade = 0;
                    for (int sample = 0; sample < steps; ++sample)
                    {
                        if constexpr (mode == ShadowMode::None)
                            scattered += transmittance * source;
                        else
                        {
                            const glm::vec3 position = frame.cameraPosition + ray * ((sample + 0.5f) * stepLength);
                            scattered += transmittance * Visibility<mode>(shadows, position, cascade) * source;
                        }
                        transmittance *= stepT;
                    }
                }
                scattered += ambientRadiance * (albedo * (1.0f - totalT));
                output[pixel] = glm::vec4(scattered, totalT);
            }
        }
    }
}

void VolumetricCPU::Integrate(const VolumetricFrame& frame, const VolumetricSettings& settings,
                             int width, int height, std::span<const float> sceneDepth,
                             std::span<const float> shadowDepth, std::span<glm::vec4> output)
{
    const size_t count = size_t(std::max(width, 0)) * std::max(height, 0);
    const bool shadowed = settings.shadows && frame.hasShadows;
    if (sceneDepth.size() != count || output.size() != count ||
        (shadowed && (frame.shadowWidth <= 0 || frame.shadowHeight <= 0 ||
         shadowDepth.size() != size_t(frame.shadowWidth) * frame.shadowHeight * 4)))
        throw std::invalid_argument("Invalid volumetric image dimensions");
    if (count == 0) return;
    if (std::max(settings.density, 0.0f) == 0.0f)
    {
        std::fill(output.begin(), output.end(), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        return;
    }
    // Dispatch once per integration, not for every ray-marching sample.
    if (!shadowed || std::clamp(settings.scatteringAlbedo, 0.0f, 1.0f) == 0.0f ||
        frame.lightRadiance == glm::vec3(0.0f))
        IntegratePixels<ShadowMode::None>(frame, settings, width, height, sceneDepth, output, Shadows{});
    else
    {
        const auto shadows = PrepareShadows(frame, settings.shadowBias, shadowDepth);
        if (HasAffineOrderedShadows(frame))
            IntegratePixels<ShadowMode::Affine>(frame, settings, width, height, sceneDepth, output, shadows);
        else
            IntegratePixels<ShadowMode::General>(frame, settings, width, height, sceneDepth, output, shadows);
    }
}
