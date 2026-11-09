#include "app/Application.h"
#include <spdlog/spdlog.h>
#include <cstddef>
#include <vector>

#include "BistroScene.h"
#include "CapturePresetScene.h"
#include "TerrainScene.h"
#include "VolumetricDemoScene.h"
#include <string_view>

int main(int argc, char** argv)
{
    spdlog::info("[TestApp] Starting");

    Application app;
    if (!app.Initialize())
    {
        spdlog::error("[TestApp] Application::Initialize() failed — aborting");
        return -1;
    }

    VolumetricDemoScene volumetricScene;
    const bool volumetricOk = volumetricScene.Setup();
    if (argc > 1 && std::string_view(argv[1]) == "--volumetric-validate")
        return volumetricOk && app.ValidateVolumetrics(volumetricScene, "build/volumetric-validation") ? 0 : 1;
    if (argc > 1 && std::string_view(argv[1]) == "--volumetric-demo")
    {
        if (!volumetricOk) return 1;
        app.Run(std::vector<Scene*>{&volumetricScene});
        return 0;
    }
    if (argc > 1 && (std::string_view(argv[1]) == "--bistro-validate" || std::string_view(argv[1]) == "--bistro-demo"))
    {
        BistroScene bistro;
        if (!bistro.Setup()) return 1;
        if (std::string_view(argv[1]) == "--bistro-validate")
            return app.ValidateVolumetrics(bistro, "build/bistro-volumetric-validation") ? 0 : 1;
        app.Run(std::vector<Scene*>{&bistro});
        return 0;
    }

    TerrainScene terrainScene;
    BistroScene bistroScene;
    CapturePresetScene captureBaseline(CapturePresetKind::Baseline);
    CapturePresetScene captureDenseGrid(CapturePresetKind::DenseGrid);
    CapturePresetScene captureMaterialSweep(CapturePresetKind::MaterialSweep);
    CapturePresetScene captureCulling(CapturePresetKind::CullingTest);

    bool terrainOk = terrainScene.Setup();
    bool bistroOk = bistroScene.Setup();
    bool captureBaselineOk = captureBaseline.Setup();
    bool captureDenseGridOk = captureDenseGrid.Setup();
    bool captureMaterialSweepOk = captureMaterialSweep.Setup();
    bool captureCullingOk = captureCulling.Setup();

    if (!terrainOk)
        spdlog::error("[TestApp] TerrainScene::Setup() failed");
    if (!bistroOk)
        spdlog::error("[TestApp] BistroScene::Setup() failed");
    if (!captureBaselineOk)
        spdlog::error("[TestApp] Capture baseline setup failed");
    if (!captureDenseGridOk)
        spdlog::error("[TestApp] Capture dense grid setup failed");
    if (!captureMaterialSweepOk)
        spdlog::error("[TestApp] Capture material sweep setup failed");
    if (!captureCullingOk)
        spdlog::error("[TestApp] Capture culling setup failed");

    std::vector<Scene*> scenes;
    if (terrainOk) scenes.push_back(&terrainScene);
    if (bistroOk) scenes.push_back(&bistroScene);
    if (captureBaselineOk) scenes.push_back(&captureBaseline);
    if (captureDenseGridOk) scenes.push_back(&captureDenseGrid);
    if (captureMaterialSweepOk) scenes.push_back(&captureMaterialSweep);
    if (captureCullingOk) scenes.push_back(&captureCulling);
    if (volumetricOk) scenes.push_back(&volumetricScene);

    for (std::size_t i = 0; i < scenes.size(); ++i)
        spdlog::info("[TestApp] Press {} for {}", i + 1, scenes[i]->GetName());

    if (!scenes.empty())
        app.Run(scenes, 0);
    else
        spdlog::error("[TestApp] No scenes were available to run");

    spdlog::info("[TestApp] Shutting down");
    return 0;
}
