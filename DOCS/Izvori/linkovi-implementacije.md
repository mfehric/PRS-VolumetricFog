# Implementacije i GPU nastavak

- **Godot 4 — Fog Volumes arrive in Godot 4.0**, Clay John, 2022. [Tehnički članak](https://godotengine.org/article/fog-volumes-arrive-in-godot-4/). Opis implementacije zasnovane na Wronskijevom pristupu.
- **Godot Engine — izvorni kod.** [Službeni repozitorij](https://github.com/godotengine/godot), [volumetric fog shader (4.0)](https://github.com/godotengine/godot/blob/4.0-stable/servers/rendering/renderer_rd/shaders/environment/volumetric_fog.glsl). Fiksirana verzija radi lakšeg praćenja implementacije iz perioda članka.
- **Mark Harris, Shubhabrata Sengupta, John D. Owens — Parallel Prefix Sum (Scan) with CUDA**, GPU Gems 3, poglavlje 39. [Cijelo poglavlje kod NVIDIA-e](https://developer.nvidia.com/gpugems/gpugems3/part-vi-gpu-computing/chapter-39-parallel-prefix-sum-scan-cuda). Referenca za scan; kod je za CUDA-u i zahtijeva prilagodbu za OpenGL compute.
- **Wronski 2014 i Hillaire 2015:** [osnovne GPU reference](Zasto%20Froxel/README.md).

Ovi izvori služe za proučavanje i budući froxel/GPU nastavak. Prva implementacijska faza sada sadrži sekvencijalni CPU i GPU ray marching; OpenMP i SIMD su naredna faza.
