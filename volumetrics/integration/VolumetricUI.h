#pragma once
#include "volumetrics/Pass.h"

// Compiled in the host: always uses the host's ImGui context and allocator.
void DrawVolumetricPanel(VolumetricSettings& settings, const VolumetricStats& stats);
void CycleVolumetricMode(VolumetricSettings& settings);
