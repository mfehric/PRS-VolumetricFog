#include "Workload.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <glm/gtc/matrix_transform.hpp>

namespace
{
    struct Box { glm::vec3 center, size; };
    // Same geometry and light/camera arrangement as VolumetricDemoScene.
    constexpr Box boxes[] = {
        {{0,-0.2f,0},{18,0.4f,18}}, {{0,0.65f,-5},{14,1.3f,0.5f}},
        {{0,6,-5},{14,2,0.5f}}, {{-5.6f,3.15f,-5},{2.8f,3.7f,0.5f}},
        {{5.6f,3.15f,-5},{2.8f,3.7f,0.5f}}, {{-1.65f,3.15f,-5},{1.3f,3.7f,0.5f}},
        {{1.65f,3.15f,-5},{1.3f,3.7f,0.5f}}, {{-7,3.5f,0},{0.4f,7,10}},
        {{0,7.1f,0},{14,0.3f,10}}, {{1.4f,1.2f,-0.5f},{1.3f,2.4f,1.3f}},
        {{-2,0.5f,1.5f},{1.4f,1,1.4f}}
    };

    float Intersect(glm::vec3 origin, glm::vec3 direction)
    {
        float closest = std::numeric_limits<float>::infinity();
        for (const auto& box : boxes)
        {
            float enter = 0.0f, leave = closest;
            for (int axis = 0; axis < 3; ++axis)
            {
                const float lo = box.center[axis] - box.size[axis] * 0.5f;
                const float hi = box.center[axis] + box.size[axis] * 0.5f;
                if (std::abs(direction[axis]) < 1e-8f)
                {
                    if (origin[axis] < lo || origin[axis] > hi) { leave = -1.0f; break; }
                    continue;
                }
                float a = (lo-origin[axis]) / direction[axis];
                float b = (hi-origin[axis]) / direction[axis];
                if (a > b) std::swap(a,b);
                enter = std::max(enter,a); leave = std::min(leave,b);
            }
            if (leave >= enter && leave > 0.0f) closest = enter;
        }
        return closest;
    }

    glm::vec3 Unproject(const glm::mat4& inverse, float x, float y, float z)
    {
        const glm::vec4 point = inverse * glm::vec4(x,y,z,1);
        return glm::vec3(point)/point.w;
    }
}

benchmark::Workload benchmark::MakeWindowRoom(int width, int height, int shadowSize)
{
    Workload result;
    result.width = width; result.height = height;
    auto& frame = result.frame;
    frame.cameraPosition = {5.4f,2.8f,3.7f};
    const glm::vec3 forward = glm::normalize(glm::vec3(-1.5f,2,-4)-frame.cameraPosition);
    frame.view = glm::lookAt(frame.cameraPosition, frame.cameraPosition+forward, {0,1,0});
    const auto vp = glm::perspective(glm::radians(52.0f), float(width)/height, 0.1f, 60.0f) * frame.view;
    frame.inverseViewProjection = glm::inverse(vp);
    frame.lightDirection = glm::normalize(glm::vec3(0.45f,-0.48f,1));
    frame.lightRadiance = glm::vec3(1,0.86f,0.64f)*10.0f;
    frame.shadowWidth = frame.shadowHeight = shadowSize;
    frame.shadowSplits = {8,18,35,60};
    frame.hasShadows = true;
    result.sceneDepth.resize(size_t(width)*height, 1.0f);
    result.output.resize(result.sceneDepth.size());
    for (int y=0; y<height; ++y)
        for (int x=0; x<width; ++x)
        {
            const auto far = Unproject(frame.inverseViewProjection, 2*(x+0.5f)/width-1, 2*(y+0.5f)/height-1, 1);
            const auto ray = glm::normalize(far-frame.cameraPosition);
            const float t = Intersect(frame.cameraPosition,ray);
            if (!std::isfinite(t)) continue;
            const auto clip = vp*glm::vec4(frame.cameraPosition+ray*t,1);
            result.sceneDepth[size_t(y)*width+x] = std::clamp(clip.z/clip.w*0.5f+0.5f,0.0f,1.0f);
        }
    result.shadowDepth.resize(size_t(shadowSize)*shadowSize*4,1.0f);
    float nearSplit = 0.1f;
    for (int cascade=0; cascade<4; ++cascade)
    {
        const float split = frame.shadowSplits[cascade];
        const auto center = frame.cameraPosition+forward*((nearSplit+split)*0.5f);
        const float extent = 6.0f+split*0.9f;
        const auto lightView = glm::lookAt(center-frame.lightDirection*80.0f,center,{0,1,0});
        const auto matrix = glm::ortho(-extent,extent,-extent,extent,0.1f,160.0f)*lightView;
        frame.shadowMatrices[cascade] = matrix;
        const auto inverse = glm::inverse(matrix);
        for (int y=0; y<shadowSize; ++y)
            for (int x=0; x<shadowSize; ++x)
            {
                const auto origin = Unproject(inverse,2*(x+0.5f)/shadowSize-1,2*(y+0.5f)/shadowSize-1,-1);
                const float t = Intersect(origin,frame.lightDirection);
                if (!std::isfinite(t)) continue;
                const auto clip = matrix*glm::vec4(origin+frame.lightDirection*t,1);
                const size_t index = (size_t(cascade)*shadowSize+y)*shadowSize+x;
                result.shadowDepth[index] = std::clamp(clip.z/clip.w*0.5f+0.5f,0.0f,1.0f);
            }
        nearSplit = split;
    }
    return result;
}

double benchmark::Checksum(std::span<const glm::vec4> output)
{
    double sum = 0;
    for (size_t i=0; i<output.size(); ++i)
    {
        const auto v = output[i];
        if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z) || !std::isfinite(v.w))
            return std::numeric_limits<double>::quiet_NaN();
        sum += (double(v.x)+2.0*v.y+3.0*v.z+5.0*v.w)*(1.0+double(i%97)/97.0);
    }
    return sum;
}
