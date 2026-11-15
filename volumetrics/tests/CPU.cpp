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

    // V0's direct world-position visibility calculation is the test oracle.
    // It deliberately does not use the prepared cascade data/affine shortcut.
    float ReferenceVisibility(const VolumetricFrame& frame, const VolumetricSettings& settings,
                              glm::vec3 position, std::span<const float> depth)
    {
        if (!settings.shadows || !frame.hasShadows) return 1.0f;
        const float viewDepth=-(frame.view*glm::vec4(position,1)).z;
        int cascade=0;
        while (cascade<4 && viewDepth>frame.shadowSplits[cascade]) ++cascade;
        if (cascade==4) return 1.0f;
        const auto clip=frame.shadowMatrices[cascade]*glm::vec4(position,1);
        if (std::abs(clip.w)<1e-8f) return 1.0f;
        const auto uvz=glm::vec3(clip)/clip.w*0.5f+0.5f;
        if (uvz.x<0 || uvz.x>=1 || uvz.y<0 || uvz.y>=1 || uvz.z<0 || uvz.z>1) return 1.0f;
        const int x=std::min(int(uvz.x*frame.shadowWidth),frame.shadowWidth-1);
        const int y=std::min(int(uvz.y*frame.shadowHeight),frame.shadowHeight-1);
        const size_t index=(size_t(cascade)*frame.shadowHeight+y)*frame.shadowWidth+x;
        return uvz.z-settings.shadowBias<=depth[index] ? 1.0f : 0.0f;
    }

    void CheckShadowRay(const VolumetricFrame& frame, const VolumetricSettings& settings,
                        const char* label)
    {
        const auto projection=glm::perspective(glm::radians(60.0f),1.0f,0.1f,100.0f);
        const auto projected=projection*glm::vec4(0,0,-24,1);
        const std::array<float,1> depth{projected.z/projected.w*0.5f+0.5f};
        std::vector<float> shadow(size_t(frame.shadowWidth)*frame.shadowHeight*4);
        for (size_t i=0; i<shadow.size(); ++i) shadow[i]=float((i*13+5)%31)/31;
        std::array<glm::vec4,1> output;
        VolumetricCPU::Integrate(frame,settings,1,1,depth,shadow,output);

        const auto world=frame.inverseViewProjection*glm::vec4(0,0,depth[0]*2-1,1);
        const auto delta=glm::vec3(world)/world.w-frame.cameraPosition;
        const float length=glm::length(delta);
        const auto ray=delta/length;
        const float distance=std::min(length,settings.maxDistance);
        const float ds=distance/settings.steps, tau=settings.density*ds;
        const float stepT=std::exp(-tau);
        const float removed=tau<0.001f ? tau*(1-0.5f*tau+tau*tau/6) : 1-stepT;
        const float g=settings.anisotropy;
        const float denominator=1+g*g-2*g*glm::dot(glm::normalize(frame.lightDirection),-ray);
        const float phase=(1-g*g)/(4*glm::pi<float>()*denominator*std::sqrt(denominator));
        const auto source=frame.lightRadiance*(phase*settings.scatteringAlbedo*removed);
        glm::vec3 scattered(0);
        float t=1;
        for (int i=0; i<settings.steps; ++i)
        {
            const auto position=frame.cameraPosition+ray*((i+0.5f)*ds);
            scattered+=t*ReferenceVisibility(frame,settings,position,shadow)*source;
            t*=stepT;
        }
        const float totalT=std::exp(-settings.density*distance);
        scattered+=settings.fogColor*(settings.ambientIntensity*settings.scatteringAlbedo*(1-totalT));
        const auto error=glm::abs(output[0]-glm::vec4(scattered,totalT));
        Require(error.x<2e-6f && error.y<2e-6f && error.z<2e-6f && error.w<2e-6f,label);
    }

    void CheckShadowPaths()
    {
        VolumetricFrame frame;
        frame.inverseViewProjection=glm::inverse(glm::perspective(glm::radians(60.0f),1.0f,0.1f,100.0f));
        frame.hasShadows=true;
        frame.shadowWidth=7; frame.shadowHeight=5;
        frame.shadowSplits={3,7,13,30};
        frame.lightRadiance={3,2,1};
        frame.shadowMatrices.fill(glm::ortho(-4.0f,4.0f,-4.0f,4.0f,0.0f,40.0f));
        VolumetricSettings settings;
        CheckShadowRay(frame,settings,"affine cascades and rectangular shadow layers match V0 visibility");
        frame.shadowSplits={3,7,13,18};
        CheckShadowRay(frame,settings,"rays beyond the last cascade match V0 visibility");
        frame.view[3][2]=-24;
        frame.view[2][2]=-1;
        CheckShadowRay(frame,settings,"decreasing view depth crosses cascades backwards correctly");
        frame.view=glm::mat4(1);
        frame.shadowSplits={7,3,13,30};
        CheckShadowRay(frame,settings,"unsorted splits preserve the general visibility path");
        frame.shadowSplits={3,7,13,30};
        for (auto& matrix: frame.shadowMatrices) matrix[2][3]=-0.03f;
        CheckShadowRay(frame,settings,"projective shadow matrices retain homogeneous division");
        for (auto& matrix: frame.shadowMatrices) { matrix[2][3]=0; matrix[3][3]=1e-10f; }
        CheckShadowRay(frame,settings,"near-zero homogeneous w retains the V0 guard");
        for (float edge: {-1.00001f,-1.0f,0.999999f,1.0f,1.00001f})
        {
            frame.shadowMatrices.fill(glm::ortho(-4.0f,4.0f,-4.0f,4.0f,0.0f,40.0f));
            for (auto& matrix: frame.shadowMatrices) matrix[3][0]=edge;
            CheckShadowRay(frame,settings,"shadow UV boundary checks retain V0 behavior");
        }
        settings.shadows=false;
        CheckShadowRay(frame,settings,"unshadowed specialization retains V0 single-scattering sum");
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
        CheckShadowPaths();

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
