# Volumetrijsko osvjetljenje — prva faza

Objedinjena verzija na branchu `cpu-volumetrics` zadržava zaseban panel i meku osnovnu maglu iz prvog rješenja, te dodaje shadowed ray marching, CPU/GPU izbor i scenu s prozorima iz drugog rješenja. Postojeći PBR lighting ostaje osnova slike. Kada je **Enable Volumetrics** isključen, volumetrijski pass se potpuno preskače i prikazuje se normal lighting.

## Pokretanje

Iz foldera `opengl-forward-renderer`, s CMake 3.26+ i C++20 kompajlerom:

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 6
.\build\test-app\TestApp.exe --volumetric-demo
```

Za Visual Studio generator putanja obično završava s `build/test-app/Release/TestApp.exe`. Na Linuxu izostaviti `-G "MinGW Makefiles"` i pokrenuti `./build/test-app/TestApp --volumetric-demo` nakon instalacije sistemskih zavisnosti iz [setup.md](setup.md).

Demo ne učitava Terrain/Bistro niti traži njihove vanjske modele. Bez argumenta sve ranije scene ostaju dostupne, a **Volumetric: Window shafts** se dodaje na kraj liste (tipka `7` kada su dostupne sve prethodne scene).

Otvoriti zaseban panel **Volumetrics**:

- **Enable Volumetrics**: uključi/isključi efekat; isključivanje zadržava odabrani CPU/GPU backend za sljedeće uključivanje.
- **Compute on**: **CPU (sequential)** ili **GPU**, vidljivo kada je efekat uključen.
- **Fog Density**, **Fog brightness** i **Fog color**: količina, osnovna rasvijetljenost i boja magle.
- **Volumetric shadows**: uključivanje zaklanjanja directional svjetla geometrijom.
- **Soft fog preset** vraća izgled meke magle sa jačim konstantnim ambientalnim izvorom; **Window shafts preset** smanjuje taj izvor radi boljeg kontrasta zraka. Preseti ne mijenjaju backend ni on/off stanje.
- **Quality and scattering**: rezolucija, broj koraka, domet, scattering albedo i `g`.
- Tipka **V** kruži: Normal → CPU → GPU → Normal.

Kamera koristi postojeće WASD/TAB kontrole. Scena ima tri otvora u zidu i kutiju koja presijeca dio snopa; prostorija je sastavljena samo od kocki. Interaktivna aplikacija počinje s uključenim efektom na GPU-u i otvorenim panelom Volumetrics. API/test konfiguracija može početi bez efekta.

Za direktan pregled Bistrou koristiti `TestApp --bistro-demo`. Efekat je isti u svim scenama; preseti su podesivi parametri istog algoritma, a ne različiti CPU/GPU modeli. Bistro koristi postojeće modele i directional light; izgled zraka zavisi od položaja kamere, prepreka i smjera tog svjetla. Nije dodat novi izvor niti promijenjeno površinsko osvjetljenje Bistrou.

## Algoritam i podaci

Za svaki piksel rekonstruisati svjetsku poziciju iz dubine i inverzne view-projection matrice. Duž zrake do prve neprozirne površine, ili do maksimalne udaljenosti, uzeti jednako razmaknute midpoint uzorke. Dubina se za obje implementacije kopira iz istog HDR depth buffera na istu nižu rezoluciju pomoću nearest blita.

Model je homogena magla i single scattering, s Henyey–Greenstein faznom funkcijom. Vrijedi `sigma_s = albedo * density`, `sigma_a = (1-albedo) * density`, `sigma_t = density`. Beer–Lambert daje `T = exp(-sigma_t * distance)`; [PBR knjiga, Transmittance](https://www.pbr-book.org/4ed/Volume_Scattering/Transmittance) daje teorijsku osnovu.

Za segment dužine `ds`:

```text
stepT = exp(-sigma_t * ds)
sourceIntegral = lightRadiance * phase * albedo * (1 - stepT)
L += T_acc * shadowVisibility(midpoint) * sourceIntegral
T_acc *= stepT
T = exp(-sigma_t * distance)
L += fogColor * ambientIntensity * albedo * (1 - T)
result = (L, T)
HDR_out = HDR_surface * result.a + result.rgb
```

Ovo je zatvoreni integral za konstantan izvor unutar segmenta; promjena vidljivosti svjetla se aproksimira midpoint uzorkom. Dodatni konstantni ambientalni izvor čuva meku maglu i u sjenovitim dijelovima scene; može se potpuno isključiti klizačem Fog brightness. To je umjetnički parametar, ne dodatno izračunato višestruko raspršenje. Za vrlo male optičke dubine koristi se stabilna polinomna aproksimacija `1-exp(-tau)`, a nulta gustoća vraća `(0,0,0,1)` bez dijeljenja nulom.

`VolumetricLighting.cpp` je čist CPU kod. **Petlja označena `SEQUENTIAL PIXEL LOOP` u `VolumetricCPU::Integrate`** računa nezavisne piksele; ona je mjesto za narednu fazu paralelizacije. Unutrašnja petlja akumulira uzorke duž jedne zrake. Trenutno nema OpenMP-a, dodatnih radnih niti ni ručnih SIMD kernela.

`VolumetricPass.cpp` preuzima scene depth i sve četiri postojeće directional shadow mape na CPU, poziva kernel, pa šalje stvarno izračunati `(L,T)` rezultat preko `glTexSubImage2D`. Readback i upload su uključeni u zasebna UI mjerenja. Svaka nova slika koristi aktuelnu kameru, objekte, svjetlo i shadow mape.

`volumetric.frag` radi isti algoritam na GPU-u i direktno uzorkuje iste dubine. Obje verzije koriste identičnu rezoluciju, broj koraka, fazu, shadow bias i nearest shadow lookup. Nemaju različite namjerne aproksimacije. Razlike su moguće zbog preciznosti i odluke na rubu hard-shadow texela.

`volumetric_composite.frag` bilinearno uvećava rezultat i blendanjem ga spaja u HDR sliku. Pass dolazi poslije svih scene draw poziva, uključujući `OnPostRender`, a prije bloom-a i tone mappinga. GL stanje se vraća nakon passa. GPU vrijeme u UI-u predstavlja CPU submission/wall time, a ne mjerenje GPU kernela.

## Provjera

```powershell
.\build\test-app\TestApp.exe --volumetric-validate
.\build\test-app\TestApp.exe --bistro-validate
```

Ovi režimi provjeravaju analitički homogeni integral, podudaranje s izvornim `MarchVolumetricRay` modelom meke magle, nultu i vrlo malu gustoću, čistu apsorpciju, gustu maglu, CPU/GPU podudaranje, uticaj prepreka, isključivanje svjetla, promjenu rezolucije i povratak na Normal Lighting. Vraćaju nenulti exit code pri neuspjehu. Slike `normal.png`, `cpu.png`, `gpu.png` nastaju u `build/volumetric-validation/` odnosno `build/bistro-volumetric-validation/`; prikazuju HDR rezultat s ACES/gamma prikazom, bez bloom-a i interfejsa. Test koristi `glFinish` samo za provjeru i vrijeme do završetka, a ne u interaktivnom renderiranju.

CPU/GPU poređenje dozvoljava malu razliku na rubovima hard-shadow texela uslijed floating-point preciznosti. CPU integration, readback i upload prikazuju se odvojeno. GPU CPU-wall-time u panelu nije vrijeme GPU kernela; za kontrolisani benchmark trebalo bi dodati GPU timestamp mjerenje i ponovljena mjerenja.

Provjereno 9. 11. 2026. na Windowsu, GCC 13.1 Release i AMD Radeon/OpenGL 4.6. Oba validation režima prošla su sve provjere, uključujući identičnu normalnu sliku nakon isključivanja efekta. Pri internoj rezoluciji 240×180 i 64 koraka:

| Scena | CPU/GPU RGBA RMSE | Maksimalna apsolutna razlika |
| :--- | ---: | ---: |
| Window shafts | `3.75e-5` | `0.00659` |
| Bistro | `1.04e-5` | `0.00142` |

Ovo su provjere funkcionalnosti i podudaranja, ne benchmark paralelnog ubrzanja. Nisu izvršene provjere na Linuxu.

## Ograničenja

- Samo directional light doprinosi volumenu. Point/spot svjetla nastavljaju osvjetljavati površine.
- Homogena magla od kamere do zadane udaljenosti; nema lokalnih volumena, tekstura gustoće, višestrukog raspršenja ni temporalne integracije.
- Sunčeva ulazna radiansa smatra se konstantnom u mediju, uz zaklanjanje geometrijom. Beer–Lambert se primjenjuje na put uzorka prema kameri; slabljenje kroz medij od svjetla do uzorka nije modelirano. Ovo je pojednostavljen vizuelni model.
- CPU se sinhronizuje s GPU-om radi čitanja dubina. Ta cijena je odvojena od sekvencijalnog kernela i mora se odvojeno analizirati pri budućem mjerenju OpenMP ubrzanja.
- Niska rezolucija i bilinearno uvećavanje mogu dati halo oko silueta. Mali broj koraka može dati pruge na rubovima sjene. Nema PCF-a u volumetrijskom lookupu niti depth-aware upsamplinga.
- Koriste se postojeća ograničenja directional CSM passa; geometrija koja se ne šalje u taj shadow pass ne zaklanja volumetrijsko svjetlo. Nema volumetrijskih sjena prozirnih površina.

Dalji froxel i paralelizacijski plan iz glavnog README-a ostaje naredna faza; ova implementacija je početna ray-marching referenca.
