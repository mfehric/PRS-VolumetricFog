# Volumetric Light and Fog — modularna referentna implementacija

Modul čuva postojeći sekvencijalni ray marching i GPU shader algoritam.
Nisu dodani OpenMP, radne niti, SIMD kerneli ni optimizacije računanja.

## Komponente i zavisnosti

| Target / folder | Uloga | Zavisnosti |
| --- | --- | --- |
| `VolumetricCPU` (SHARED) | Jedina kompajlirana CPU implementacija `VolumetricCPU::Integrate` | C++20, GLM 1.0.1, C++ runtime |
| `VolumetricFog` (SHARED) | OpenGL pass, depth/shadow readback, CPU/GPU izbor, upload, HDR spajanje i mjerenja | CPU DLL, hostov GLAD target i aktivan GL kontekst |
| `VolumetricBenchmark` | Samostalan EXE za CPU računanje | CPU DLL; tokom rada nema OpenGL/GLFW/ImGui zavisnosti |
| `integration/` | Host adapter, UI, validacija, demo scena i mali patch originalnog renderera | Postojeći RendererLib, GLFW, ImGui, stb |
| `assets/` | Dva volumetrijska fragment shadera | GPU backend i compositing |

Matematika je u `src/Lighting.cpp`; javni tipovi i eksportovana funkcija su u
`include/volumetrics/Lighting.h`. `src/Pass.cpp` poziva taj isti DLL kod u CPU
režimu. Benchmark ga također importuje iz istog DLL-a: nema druge kopije algoritma.
`Ray.h` čuva početni model meke magle za provjere kompatibilnosti.

Odvojeni CPU DLL omogućava headless build bez grafičkog stacka. GPU i OpenGL
resursi ostaju u drugom DLL-u. ShaderProgram, FullscreenQuad, ostatak renderera i
ImGui nisu kopirani u DLL: `GraphicsServices.h` omogućava adapteru da koristi
postojeće host implementacije. UI se kompajlira u hostu da koristi njegov ImGui
kontekst. DLL inicijalizira vlastite GLAD funkcijske pokazivače preko loadera
koji dobije od hosta. Vlasnik GL konteksta mora uništiti pass prije konteksta.

Ovo je C++ API za povezivanje pri buildu, a ne `LoadLibrary` plugin sa stabilnim
C ABI-jem. Renderer, DLL-ove i benchmark buildati kompatibilnim kompajlerom,
arhitekturom, C++ runtimeom i GLM opcijama. Ne miješati MSVC i MinGW binarne
fajlove. CMake vodi import biblioteke i linkovanje; nije potrebno ručno podešavati
DLL eksport simbole. MinGW generiše `libVolumetricCPU.dll` i
`libVolumetricFog.dll`; MSVC `VolumetricCPU.dll` i `VolumetricFog.dll`.

## CPU build i benchmark (iz korijena repozitorija)

Potreban je CMake 3.26+ i C++20 kompajler. Prvi build preuzima GLM, bez zavisnosti
renderera. Na Windowsu s postojećim MSYS2 UCRT64/MinGW toolchainom:

```powershell
cmake -S . -B build-cpu -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DVOLUMETRICS_BUILD_RENDERER=OFF
cmake --build build-cpu --parallel 6
ctest --test-dir build-cpu --output-on-failure
.\build-cpu\volumetrics\VolumetricBenchmark.exe --iterations 1000 --warmup 3 --width 240 --height 180 --steps 64 --shadow-size 1024 --csv build-cpu/timings.csv
```

Za PDB simbole na Windowsu može se koristiti MSVC (Developer PowerShell):

```powershell
cmake -S . -B build-cpu-msvc -G "Visual Studio 17 2022" -A x64 -DVOLUMETRICS_BUILD_RENDERER=OFF
cmake --build build-cpu-msvc --config Release --parallel 6
ctest --test-dir build-cpu-msvc -C Release --output-on-failure
.\build-cpu-msvc\volumetrics\Release\VolumetricBenchmark.exe --iterations 1000
```

I `cmake -S volumetrics -B build-module -DVOLUMETRICS_BUILD_OPENGL=OFF` podržava
samostalan CPU modul; u tom slučaju izvršni fajlovi su direktno u `build-module/`.
Opcija `BUILD_TESTING=ON` uključuje testove kada se modul dodaje iz glavnog builda.
Na Linuxu odgovarajući fajl je `VolumetricBenchmark`, uz `.so` biblioteke.

CPU DLL se generiše pored benchmarka. MinGW runtime DLL-ovi moraju biti dostupni
u `PATH` (npr. `C:\msys64\ucrt64\bin`) ili uz EXE. Benchmark se može pokrenuti
iz bilo kojeg radnog foldera; ne učitava assete ni modele s diska. CSV folder mora
već postojati. Za sve opcije koristiti `--help`.

## Šta benchmark mjeri

`benchmark/Workload.cpp` deterministički pravi prostoriju s tri otvora i
preprekama, koristeći geometriju i raspored kamere/svjetla iz demo scene.
Presjecima zraka i kutija generiše scene depth i četiri stvarne directional
shadow depth slike, s odgovarajućim matricama i cascade splitovima. Format je isti
kao u rendereru: bottom-left origin, depth `[0,1]`, OpenGL clip Z `[-1,1]`.
Shadow matrice fixturea su determinističke; ovo nije bit-identičan snimak
rendererovog CSM passa. Test potvrđuje da prepreke stvarno zaklanjaju svjetlost.

Generisanje ulaza i alokacije završavaju prije mjerenja. Nakon warmupa se ponavlja
isključivo poziv `VolumetricCPU::Integrate`; ulazi se ponovo koriste da bi se dobio
ponovljiv workload. Za svaku iteraciju se izvan mjerenog intervala čitaju svi
izlazni pikseli i računa checksum. Provjeravaju se konačne vrijednosti i identičan
rezultat ponavljanja. Time se rezultat koristi, a odvojeni DLL i isključen LTO
dodatno sprečavaju uklanjanje računanja.

Ispis sadrži parametre, setup vrijeme, ukupan/prosječan/medijalni/min/max kernel
interval, procijenjeni broj pixel-sample obrada u sekundi, checksum i jedan
izlazni `(L.r,L.g,L.b,T)` piksel. CSV sadrži interval i checksum svake iteracije
i parametre. Throughput nije hardverski brojač instrukcija.
Profiliranje cijelog EXE-a vidi i setup/warmup/checksum; za kernel analizu gledati
`VolumetricCPU::Integrate` i njegove pozive u CPU DLL-u. Checksum prolaz također
utiče na cache stanje između ponavljanja; to je dio fiksnog benchmark protokola.
Nisu uključeni rendererov GPU readback/upload ni UI; oni se zasebno mjere u rendereru.

## Renderer i demo

Postojeći način builda ostaje dostupan iz korijena repozitorija:

```powershell
cmake -S opengl-forward-renderer -B opengl-forward-renderer/build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build opengl-forward-renderer/build --parallel 6
ctest --test-dir opengl-forward-renderer/build --output-on-failure
Push-Location opengl-forward-renderer/build/test-app
.\TestApp.exe --volumetric-demo
# ili: .\TestApp.exe --volumetric-validate
# ili: .\TestApp.exe --bistro-validate  (traži postojeće Bistro modele)
Pop-Location
```

Izvršni folder sadrži oba DLL-a i `assets/volumetrics/shaders/`. Originalni
`assets/shaders/tone_mapping.vert` koristi se kroz hostove shader servise.
Run iz izvršnog foldera osigurava dostupnost svih renderer asseta. Scene, oba
backend-a, panel, preseti, V prečica i validacijski režimi su sačuvani.

Za razvojni run iz izvornog foldera renderera adapter može pronaći shadere u
izvornom folderu modula. Za prenošenje binarnih fajlova koristiti kopirane assete
uz EXE; izvorni folder tada nije potreban.

Alternativni puni build: `cmake -S . -B build-renderer -DCMAKE_BUILD_TYPE=Release` pa
`cmake --build build-renderer --config Release`; TestApp je tada u
`build-renderer/opengl-forward-renderer/test-app/` (plus `Release/` za MSVC).

## Integracija u originalni renderer

Pregledani original je
[`bakir004/opengl-forward-renderer`, `90ccaa4`](https://github.com/bakir004/opengl-forward-renderer/commit/90ccaa40b28f90fc3772b44e94031600930c5493).
Naš početni projekat je
[`mfehric/PRS-VolumetricFog`, `04a6048`](https://github.com/mfehric/PRS-VolumetricFog/commit/04a6048b3e78593b1a28efbc339a51fdcbb598ef).
Izvan volumetrijskih izmjena rendererov `src` odgovara originalu. Potrebni su
samo hookovi u 11 postojećih fajlova; kompletna implementacija dolazi iz ovog
foldera. Ne prenositi stare izmjene ray marchinga i shadera pojedinačno.

1. Prenijeti samo folder `volumetrics/` ili ga ostaviti kao susjedni projekat.
2. Na čistom originalu primijeniti `integration/upstream.patch`, npr. iz ovog repoa:

```powershell
.\volumetrics\integration\apply-upstream.ps1 -RendererPath C:/putanja/original-renderer
cmake -S C:/putanja/original-renderer -B C:/putanja/original-build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DPRS_VOLUMETRICS_DIR=C:/putanja/PRS-VolumetricFog/volumetrics
cmake --build C:/putanja/original-build --parallel 6
ctest --test-dir C:/putanja/original-build --output-on-failure
```

Skripta prvo provjerava patch i ne kreira commit. Patch je vezan za navedeni
upstream; kod drugih verzija `git apply --check` može zahtijevati prilagodbu.
`Integration.cmake` registruje adapter, UI/validaciju, demo, linkove i kopiranje
resursa/DLL-ova. CMake dodaje modul nakon definisanja hostovih zavisnosti.

Host hookovi daju passu HDR framebuffer, kameru, directional light i postojeće
četiri shadow kaskade. Pass se poziva poslije `OnPostRender`, prije bloom-a i tone
mappinga. Ostatak originalnog renderer pipelinea ostaje u rendereru.

## AMD uProf priprema

`VOLUMETRICS_PROFILE_SYMBOLS=ON` je podrazumijevano: Release zadržava optimizacije
kompajlera i dodaje `-g -fno-omit-frame-pointer` za GCC/Clang, odnosno `/Zi /Oy-`
i `/DEBUG:FULL` za MSVC. LTO je isključen za modul i benchmark. EXE i DLL ne
stripati; PDB fajlove čuvati uz pripadajuće MSVC binarne fajlove i izvorni kod.
AMD opisuje [Windows PDB postavke](https://docs.amd.com/r/en-US/57368-uProf-user-guide/Generating-Debug-Information-on-Windows).
MinGW build sadrži DWARF simbole; treba provjeriti njihovo razrješavanje u
konkretnoj uProf verziji. MSVC varijanta daje PDB, ali lokalna provjera tog
toolchaina nije pretpostavljena.

U profileru odabrati benchmark EXE, njegove argumente i direktorij binarnih
fajlova/simbola. CPU DLL je glavni interes za hotspots. Početi vremenskim
samplingom, zatim provjeriti dostupne PMC/IBS metrike za konkretan Ryzen/OS/driver.
Ne obećavaju se svi brojači cache missova, latencije ili instrukcija na svakoj
konfiguraciji. AMD ima [pregled CPU sampling metoda](https://docs.amd.com/r/en-US/57368-uProf-user-guide/7.9.1.-Overview)
i [IBS analize](https://docs.amd.com/r/en-US/57368-uProf-user-guide/Analysis-with-Instruction-Based-Sampling).
Ako hardverski sampling na Windowsu nije dostupan, provjeriti prijavu alata i
[poznata ograničenja](https://docs.amd.com/r/en-US/63856-uProf-release-notes/CPU-Profiling),
uključujući Hyper-V; ovaj projekat ne mijenja sistemske postavke.

Prije poređenja optimizacija sačuvati prvu sekvencijalnu verziju i metapodatke
u [optimization/](../optimization/README.md). Profiliranje i optimizacije nisu
dio ove modularizacije. Stvarno izvršene provjere su u [VALIDATION.md](VALIDATION.md).
