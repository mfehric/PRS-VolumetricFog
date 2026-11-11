#pragma once

#include <cmath>
#include <glm/glm.hpp>

struct VolumetricRayResult
{
    float transmittance = 1.0f;
    float inScattering = 0.0f;
};

inline VolumetricRayResult MarchVolumetricRay(float rayLength,
                                             float fogDensity,
                                             float lightIntensity,
                                             int stepCount)
{
    VolumetricRayResult result;
    if (rayLength <= 0.0f || fogDensity <= 0.0f || stepCount <= 0)
        return result;

    const float stepLength = rayLength / static_cast<float>(stepCount);
    const float stepTransmittance = std::exp(-fogDensity * stepLength);

    for (int step = 0; step < stepCount; ++step)
    {
        result.inScattering += result.transmittance
                             * (1.0f - stepTransmittance)
                             * lightIntensity;
        result.transmittance *= stepTransmittance;
    }

    return result;
}

inline float GetVolumetricRayLength(const glm::vec2 &pixelUv,
                                    float depth,
                                    const glm::mat4 &inverseViewProjection,
                                    const glm::vec3 &cameraPosition,
                                    float maxRayLength)
{
    if (depth >= 1.0f)
        return maxRayLength;

    const glm::vec2 ndc = pixelUv * 2.0f - glm::vec2(1.0f);
    const glm::vec4 clipPosition(ndc, depth * 2.0f - 1.0f, 1.0f);
    const glm::vec4 worldPosition = inverseViewProjection * clipPosition;
    const glm::vec3 surfacePosition = glm::vec3(worldPosition) / worldPosition.w;

    return glm::min(glm::length(surfacePosition - cameraPosition), maxRayLength);
}