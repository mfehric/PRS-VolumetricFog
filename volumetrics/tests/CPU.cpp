#include "../benchmark/Workload.h"
#include "volumetrics/Ray.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

namespace
{
    void Require(bool ok, const char* message)
    {
        if (!ok) throw std::runtime_error(message);
        std::cout << "PASS: " << message << '\n';
    }
}

int main()
{
    try
    {
        VolumetricFrame frame;
        const auto projection=glm::perspective(glm::radians(60.0f),1.0f,0.1f,100.0f);
        frame.inverseViewProjection=glm::inverse(projection);
        frame.lightRadiance=glm::vec3(3);
        const auto clip=projection*glm::vec4(0,0,-10,1);
        const std::array<float,1> depth{clip.z/clip.w*0.5f+0.5f};
        std::array<glm::vec4,1> output;
        VolumetricSettings settings;
        settings.density=0.1f; settings.anisotropy=0; settings.ambientIntensity=0;
        auto integrate=[&] { VolumetricCPU::Integrate(frame,settings,1,1,depth,{},output); };
        integrate();
        const float t=std::exp(-1.0f), light=3*settings.scatteringAlbedo/(4*glm::pi<float>())*(1-t);
        Require(std::abs(output[0].w-t)<2e-5f && std::abs(output[0].x-light)<2e-5f,"analytic homogeneous integral");
        settings.density=0; integrate();
        Require(output[0]==glm::vec4(0,0,0,1),"zero density identity");
        settings.density=0.1f; settings.scatteringAlbedo=0; integrate();
        Require(output[0].x==0 && std::abs(output[0].w-t)<2e-5f,"pure absorption");
        settings.density=1e-8f; settings.scatteringAlbedo=0.9f; integrate();
        Require(std::isfinite(output[0].x) && output[0].x>0 && output[0].w>0.999f,"near-zero density");
        settings.density=10; integrate();
        Require(std::isfinite(output[0].x) && output[0].w<1e-20f,"dense fog");
        frame.lightRadiance=glm::vec3(0); settings.density=0.1f;
        settings.scatteringAlbedo=1; settings.ambientIntensity=0.35f; integrate();
        const auto original=MarchVolumetricRay(10,0.1f,0.35f,settings.steps);
        Require(std::abs(output[0].w-original.transmittance)<2e-5f && std::abs(output[0].x-original.inScattering)<2e-5f,"original soft fog model");
        bool rejected=false;
        try { VolumetricCPU::Integrate(frame,settings,2,1,depth,{},output); }
        catch (const std::invalid_argument&) { rejected=true; }
        Require(rejected,"invalid image dimensions rejected");

        auto scene=benchmark::MakeWindowRoom(80,60,128);
        auto again=benchmark::MakeWindowRoom(80,60,128);
        Require(scene.sceneDepth==again.sceneDepth && scene.shadowDepth==again.shadowDepth,"deterministic geometric depth fixture");
        settings=VolumetricSettings{};
        auto render=[&] { VolumetricCPU::Integrate(scene.frame,settings,80,60,scene.sceneDepth,scene.shadowDepth,scene.output); };
        render(); const auto shadowed=scene.output;
        Require(std::isfinite(benchmark::Checksum(shadowed)),"finite shadowed window room");
        size_t litPixels=0;
        for (const auto value : shadowed)
        {
            const float ambient=settings.fogColor.x*settings.ambientIntensity*settings.scatteringAlbedo*(1-value.w);
            if (value.x>ambient+1e-5f) ++litPixels;
        }
        Require(litPixels>shadowed.size()/50,"fixture contains direct light as well as shadow");
        render(); Require(shadowed==scene.output,"repeatable integration");
        settings.shadows=false; render();
        double shadowedL=0, unshadowedL=0;
        size_t affected=0;
        for (size_t i=0; i<scene.output.size(); ++i)
        {
            shadowedL+=shadowed[i].x; unshadowedL+=scene.output[i].x;
            if (scene.output[i].x>shadowed[i].x+1e-6f) ++affected;
        }
        Require(affected>scene.output.size()/20 && unshadowedL>shadowedL+1,"fixture exercises geometry shadows");
        std::cout << "Shadow-affected pixels: " << affected << '/' << scene.output.size() << '\n';
        std::cout << "Pixels with direct scattering: " << litPixels << '/' << scene.output.size() << '\n';
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
