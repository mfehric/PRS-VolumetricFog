#pragma once
#include "volumetrics/Lighting.h"

namespace benchmark
{
    struct Workload
    {
        VolumetricFrame frame;
        std::vector<float> sceneDepth;
        std::vector<float> shadowDepth;
        std::vector<glm::vec4> output;
        int width = 0, height = 0;
    };

    // Procedural window room, rendered into CPU depth images BEFORE timing.
    // Bottom-left origin, OpenGL [-1,1] clip Z, four directional cascades.
    Workload MakeWindowRoom(int width, int height, int shadowSize);
    double Checksum(std::span<const glm::vec4> output);
}
