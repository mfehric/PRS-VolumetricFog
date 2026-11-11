# Provjera modularizacije — 11. 11. 2026.

Izmjene su lokalne. Nisu kreirani commit, push ni pull request. Profiliranje i
optimizacije nisu izvršeni; ovaj dokument evidentira funkcionalne provjere.

## Okruženje

Windows, MSYS2 UCRT64 GCC 13.1.0, C++20, CMake 4.4, MinGW Makefiles, Release
(`-O3 -DNDEBUG`, dodatni `-g -fno-omit-frame-pointer` za CPU DLL i benchmark).
Renderer je radio na AMD Radeon (TM) Graphics / OpenGL 4.6.
CPU je AMD Ryzen 7 7730U. Buildovi su koristili postojeće FetchContent source
direktorije zavisnosti; upstream renderer je zasebno kloniran i buildan.

## Stvarno izvršene provjere

| Provjera | Rezultat |
| --- | --- |
| CPU-only build iz root CMake-a | `VolumetricCPU` DLL, benchmark i CPU test EXE uspješno buildani |
| Headless CTest | 3/3: fizikalni model, deterministički fixture i sjene, benchmark smoke, neispravan CLI ulaz |
| Benchmark 1.000 iteracija | 240×180, 64 koraka, 4×1024² shadow podaci; exit 0, CSV sa 1.000 redova i istim checksumom po iteraciji |
| DLL naspram starog CPU koda | Cijeli izlaz identičan za 120×90 piksela u šest scenarija (sjene on/off, nulta/mala gustoća, apsorpcija, izmijenjeni parametri) |
| Lokalni renderer build | Oba DLL-a i TestApp uspješno buildani i povezani |
| Radni folder | Validacija uspješna i iz izvršnog foldera i iz izvornog foldera renderera |
| Renderer CTest | 4/4, uključujući stvarni OpenGL CPU/GPU validation run |
| Bistro | `--bistro-validate` uspješan s lokalno dostupnim modelima |
| Čisti upstream + patch + modul | Uspješan nezavisan build i 4/4 CTest na originalu `90ccaa40b28f90fc3772b44e94031600930c5493` |
| GPU izgled | Otvorene i pregledane generisane CPU/GPU slike window scene; vidljivi isti otvori, magla i snopovi |
| Runtime zavisnosti benchmarka | `objdump -p`: CPU DLL i C++/Windows runtime; nema OpenGL/GLFW/ImGui/renderer DLL zavisnosti |
| Simboli | `objdump -h` potvrđuje `.debug_info`, `.debug_line` u MinGW CPU DLL-u; provjerene Release compile opcije |

Alternativni puni root CMake entry point (`build-renderer/`) uspješno je
konfigurisan/generisan; puni renderer build i izvršavanje provjereni su preko
postojećeg `opengl-forward-renderer/build/` entry pointa i preko upstream builda.

Izvor `Lighting.cpp`, izvorni `Ray.h`, oba fragment shadera i demo geometrija
provjereni su prema `04a6048`: algoritam/shaderi nisu izmijenjeni (osim putanje
include-a u CPU fajlu). Privremeni reference EXE kompajliran je iz stare verzije
CPU fajla, uz novi fixture, i poređen s pozivom DLL-a. Referentni algoritam nije
dupliciran u proizvodnom kodu ni benchmarku.

CPU/GPU provjere uključuju Beer–Lambert/single-scattering analitički rezultat,
nultu i vrlo malu gustoću, gustu maglu, apsorpciju, originalni soft-fog model,
prepreke, uklanjanje svjetla, resize/quality promjene i identičnu normalnu sliku
nakon isključivanja efekta.

| Scena | CPU/GPU RGBA RMSE | Maksimalna razlika |
| --- | ---: | ---: |
| Window shafts | `3.749734e-5` | `0.006586712` |
| Bistro | `1.035462e-5` | `0.001420662` |

Hard-shadow rubovi imaju postojeću toleranciju zbog floating-point preciznosti.
Ovo su provjere podudaranja; nisu mjerenja ubrzanja.

## Gdje su lokalni dokazi

- `build-cpu/Testing/Temporary/LastTest.log`
- `build-cpu/validation-1000.csv` (funkcionalni run, ne baseline; druge provjere/buildovi su radili istovremeno)
- `opengl-forward-renderer/build/Testing/Temporary/LastTest.log`
- `opengl-forward-renderer/build/source-directory-validation.log`
- `opengl-forward-renderer/build/test-app/build/volumetric-validation/{normal,cpu,gpu}.png`
- `opengl-forward-renderer/build/test-app/bistro-modular-validation.log`
- `opengl-forward-renderer/build/test-app/build/bistro-volumetric-validation/{normal,cpu,gpu}.png`
- `build-upstream/Testing/Temporary/LastTest.log`
- `.integration-work/upstream/` — zasebna kopija originalnog repoa s primijenjenim patchem
- `.integration-work/Reference{,Main}.cpp` i `ReferenceParity.exe` — privremena provjera starog kernela

Build folderi i privremeni upstream nisu dio izvornog paketa i ignorisani su u Gitu.
U `optimization/` su samo predlošci: nema izmišljenih baseline mjerenja.

## Šta nije provjereno

Nisu izvršeni AMD uProf sampling, hardverski brojači, MSVC/PDB build ni Linux
build. Nije urađen dug interaktivni walkthrough svih scena i UI kontrola;
CPU/GPU rad i lifecycle provjereni su automatizovanim OpenGL režimima i slikama.
Primjena patcha na druge upstream commitove nije garantovana.
