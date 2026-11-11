#include "VolumetricUI.h"
#include <imgui.h>
#include <spdlog/spdlog.h>

void CycleVolumetricMode(VolumetricSettings& fog)
{
    if (!fog.enabled) { fog.enabled = true; fog.backend = VolumetricBackend::CPU; }
    else if (fog.backend == VolumetricBackend::CPU) fog.backend = VolumetricBackend::GPU;
    else fog.enabled = false;
    spdlog::info("[Volumetric] Mode: {}", !fog.enabled ? "Normal Lighting" :
                 fog.backend == VolumetricBackend::CPU ? "Volumetric CPU" : "Volumetric GPU");
}

void DrawVolumetricPanel(VolumetricSettings& volumetric, const VolumetricStats& timing) {
    ImGui::TextUnformatted("Volumetric Light and Fog");
    ImGui::Checkbox("Enable Volumetrics", &volumetric.enabled);
    if (!volumetric.enabled) {
        ImGui::TextWrapped("Normal lighting. Press V to cycle Normal / CPU / GPU.");
        return;
    }
    int backend = static_cast<int>(volumetric.backend);
    if (ImGui::Combo("Compute on", &backend, "CPU (sequential)\0GPU\0"))
        volumetric.backend = static_cast<VolumetricBackend>(backend);
    ImGui::SliderFloat("Fog Density", &volumetric.density, 0.0f, 0.2f, "%.3f");
    ImGui::SliderFloat("Fog brightness", &volumetric.ambientIntensity, 0.0f, 0.6f, "%.3f");
    ImGui::ColorEdit3("Fog color", &volumetric.fogColor.x);
    ImGui::Checkbox("Volumetric shadows", &volumetric.shadows);
    ImGui::Spacing();
    if (ImGui::Button("Soft fog preset")) {
        volumetric.density = 0.04f; volumetric.ambientIntensity = 0.35f;
        volumetric.fogColor = glm::vec3(1.0f); volumetric.scatteringAlbedo = 1.0f;
        volumetric.anisotropy = 0.0f; volumetric.maxDistance = 50.0f;
    }
    if (ImGui::Button("Window shafts preset")) {
        volumetric.density = 0.065f; volumetric.ambientIntensity = 0.025f;
        volumetric.fogColor = glm::vec3(1.0f); volumetric.scatteringAlbedo = 0.9f;
        volumetric.anisotropy = 0.2f; volumetric.maxDistance = 35.0f;
        volumetric.shadows = true; volumetric.width = 240; volumetric.steps = 96;
    }
    if (ImGui::CollapsingHeader("Quality and scattering")) {
        ImGui::SliderInt("Internal width", &volumetric.width, 64, 640);
        ImGui::SliderInt("Ray samples", &volumetric.steps, 8, 128);
        ImGui::SliderFloat("Fog distance", &volumetric.maxDistance, 1.0f, 100.0f, "%.1f");
        ImGui::SliderFloat("Scattering albedo", &volumetric.scatteringAlbedo, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Anisotropy g", &volumetric.anisotropy, -0.9f, 0.9f, "%.2f");
    }
    ImGui::Spacing();
    if (timing.active) {
        ImGui::Text("Effect: %d x %d", timing.width, timing.height);
        if (volumetric.backend == VolumetricBackend::CPU) {
            ImGui::Text("CPU integration: %.2f ms", timing.cpuIntegrationMs);
            ImGui::Text("Readback: %.2f ms", timing.readbackMs);
            ImGui::Text("Upload: %.2f ms", timing.uploadMs);
        } else ImGui::TextWrapped("GPU rendering; CPU does not integrate the fog.");
        ImGui::Text("CPU pass wall time: %.2f ms", timing.totalCpuMs);
    }
    ImGui::TextWrapped("V: Normal / CPU / GPU. Scenes: Volumetric: Window shafts.");
}

