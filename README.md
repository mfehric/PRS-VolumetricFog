# Volumetrijsko svjetlo i magla (P19)

Projekat iz predmeta **Paralelni računarski sistemi**, tema **P19: Volumetric Light and Fog**. Nadovezuje se na naš OpenGL forward renderer.

Cilj je implementirati volumetrijsku maglu i svjetlo (snopovi svjetla, "god rays") po **froxel** pristupu (Wronski 2014, Hillaire 2015). Prvo pravimo sekvencijalnu CPU verziju, a zatim je paralelizujemo pomoću OpenMP-a i SIMD-a (AVX2). GPU verzija je tema drugog projekta.

## Izvori

Folder `Izvori/` sadrži naučne radove, slajdove i linkove. Radovi na kojima se zasniva prezentacija su označeni sa ★.

- **Pregled metoda/**: pregled oblasti i drugih pristupa, da opravdamo zašto ih ne biramo.
  - *Volumetric Light Scattering as a Post-Process* (Mitchell, GPU Gems 3, pogl. 13, 2008): radial blur u screen-spaceu. Jeftino, ali nije fizikalno tačno.
  - Ray marching kroz shadow map po pikselu: tačno, ali broj shadow-map uzoraka raste s brojem piksela × koraka.
  - *Real Time Volumetric Shadows using Polygonal Light Volumes* (HPG 2010): analitički pristup bez ray marchinga (provjeriti autore prije citiranja).
  - *Real-Time Multiple Scattering using Light Propagation Volumes* (I3D 2012): aproksimacija višestrukog raspršenja.
- **Zasto Froxel/**: radovi koji su osnova za izabrani pristup.
  - ★ *Volumetric Fog: Unified Compute Shader Based Solution to Atmospheric Scattering* (Wronski, SIGGRAPH 2014): **osnova algoritma**. Slajd "Why not 2D raymarching?" je izvor kriterija poređenja ray marching naspram froxela.
  - ★ *Physically Based and Unified Volumetric Rendering in Frostbite* (Hillaire, SIGGRAPH 2015): fizikalni parametri, volumetrijske sjene, temporalna integracija.
  - Napomena: oba rada opisuju GPU implementaciju (compute shaderi). Sam algoritam (injection, integracija po dubini, primjena) ne zavisi od hardvera, pa ga ovdje preslikavamo na CPU. Specifičan rad o froxel magli na CPU-u nismo našli, što treba navesti kao ograničenje literature.
- **Teorija/**
  - ★ *Monte Carlo Methods for Physically Based Volume Rendering* (Novák et al., SIGGRAPH 2018 course): jednačina prijenosa zračenja, Beer-Lambert, fazne funkcije.
- **Paralelizacije (CPU)/**: literatura za paralelizaciju volumetrijskog renderiranja na CPU-u.
  - *Fast CPU Volume Rendering with Coherent BVH Traversal* (Knoll et al.): CPU volume ray casting s višenitnim packet ray tracerom i SSE vektorskim instrukcijama za integraciju. Zaključak autora je da optimizovan CPU renderer može biti konkurentan GPU-u (provjeriti tačan naziv rada i konferenciju prije citiranja).
  - *Volume Rendering on Scalable Shared-Memory MIMD Architectures* (Nieh, Levoy, 1992): paralelni volume rendering na višeprocesorskim mašinama sa zajedničkom memorijom, uz task queue podjelu slike. Direktno povezano s našim izborom `static` i `dynamic` rasporeda u OpenMP-u.
  - *ispc: A SPMD Compiler for High-Performance CPU Programming* (Pharr, Mark, InPar 2012): SPMD-on-SIMD model, jedna instanca programa po SIMD lane-u. Opciona referenca ako SIMD radimo i izvan intrinsics-a.
  - *Embree: A Kernel Framework for Efficient CPU Ray Tracing* (Wald et al., ACM TOG 2014): primjer iskorištavanja višejezgrenosti i širokih SIMD jedinica (SSE, AVX, AVX2) u CPU renderingu. Koristimo je kao referencu za projektovanje SIMD kernela, ne za sam algoritam.
  - *Memory Layout Transformations* (Intel): pretvaranje AoS u SoA da se izbjegnu gather i scatter operacije u vektorizovanom kodu. Opravdanje za naš SoA raspored.
- `linkovi-implementacije.md`: tuđi repozitoriji i implementacije (npr. Godot 4 volumetric fog).

## Prezentacija (3. 11. 2026)

Folder `Prezentacija/`:
- `P19_Volumetric_Fog_and_Light.pptx`: slajdovi (10 min, 3 govornika).

Sadržaj: fizika (atmospheric scattering, Beer-Lambert), primjeri, ray marching po pikselu (referentna implementacija), froxel (glavni pristup), plan paralelizacije na CPU-u (OpenMP + SIMD).

## Teorija u kratkim crtama

Ekstinkcija je zbir apsorpcije i raspršenja, `sigma_t = sigma_a + sigma_s`. Transmitansa (Beer-Lambert):

```
T(a, b) = exp( - integral_a^b sigma_t(s) ds )      // homogeno: T = exp(-sigma_t * d)
```

Fazna funkcija (Henyey-Greenstein), `g > 0` unaprijed, `g = 0` izotropno, `g < 0` unazad:

```
p(theta) = (1 / 4pi) * (1 - g^2) / (1 + g^2 - 2 g cos(theta))^(3/2)
```

Single scattering duž zrake do dubine `s`:

```
L = L_surf * T(0, s) + integral_0^s T(0, t) * sigma_s(t) * p(theta) * V(t) * L_light(t) dt
```

Ovdje je `V(t)` vidljivost svjetla (shadow map). Unutar jednog sloja froxel grida debljine `dz`, uz konstantne `S` (raspršeno svjetlo) i `sigma_t`, integral ima zatvoreni oblik koji čuva energiju:

```
T_i      = exp(-sigma_t * dz)
doprinos = (S - S * T_i) / sigma_t        // za sigma_t -> 0 granična vrijednost je S * dz
```

## Početni plan implementacije

C++20 i CMake. Froxel grid je `W × H × D` ćelija poravnat s frustumom kamere (rezolucija je parametar, npr. 160×90×64; provjeriti u Wronskijevim slajdovima). Podaci su u **SoA** rasporedu (posebni nizovi za `sigma_t`, `S` i `T`) zbog SIMD-a.

1. **Sekvencijalna verzija.**
   1. *Injection*: za svaku ćeliju gustoća `sigma_s`, `sigma_t`, vidljivost iz shadow mape, faza `p(theta)`, što daje `S`.
   2. *Integracija po dubini*: za svaki stupac (x,y) front-to-back, uz `L_acc += T_acc * (S - S*T_i)/sigma_t` i `T_acc *= T_i`.
   3. *Primjena*: fragment shader očitava `(L_acc, T)` trilinearno i računa `C = C_surf * T + L_acc`.
   Ulaz: testna scena i shadow mapa. Izlaz: 3D tekstura (`glTexSubImage3D`) i slika s maglom.
2. **SIMD.** Kernel injectiona i integracije u tri verzije: skalarna, `#pragma omp simd` i ručni AVX2/FMA.
3. **OpenMP.** Injection: `parallel for` po (z,y), SIMD po x. Integracija: paralelno po (x,y) stupcima (`static` i `dynamic` raspored), sekvencijalno po z. Moguća blokovska (tiled) podjela zbog keša.
4. **Mjerenja.** Rezolucija grida, broj niti, speedup i efikasnost u odnosu na sekvencijalnu verziju, strong i weak scaling, poređenje skalarne, SIMD i AVX2 verzije.

## Nastavak: GPU verzija (kasnije)

GPU implementacija dolazi u drugom projektu, pa je ovdje ostavljamo kao smjer nastavka. Cilj je da CPU verzija iz ovog projekta bude **referenca ispravnosti i baseline za poređenje** (isti `sigma`, `g`, svjetlo, ista scena).

Plan prenosa na GPU (OpenGL compute shaderi):
- *Injection*: jedna nit po froxelu (3D dispatch), isti kernel kao CPU verzija.
- *Integracija po dubini*: jedna nit po (x,y) stupcu. Ako je stupaca premalo za zauzetost GPU-a, koristiti **parallel prefix scan** jer se segmenti komponuju asocijativno, `(L1,T1) o (L2,T2) = (L1 + T1*L2, T1*T2)`.
- *Primjena*: forward shader očitava 3D teksturu, što je isto kao u CPU verziji, samo bez transfera.
- Poređenje: CPU (OpenMP, AVX2) naspram GPU po rezoluciji grida, uz mjerenje vremena kernela i transfera zasebno.

Izvori za GPU dio:
- Wronski (SIGGRAPH 2014) i Hillaire (SIGGRAPH 2015), isti kao gore, jer su to originalne GPU implementacije.
- Godot 4 volumetric fog (članak "Fog volumes arrive in Godot 4"): otvorena implementacija zasnovana na Wronskijevom pristupu.
- Harris, Sengupta, Owens, *Parallel Prefix Sum (Scan) with CUDA* (GPU Gems 3): standardna referenca za scan na GPU-u. Navedeno iz sjećanja, provjeriti prije citiranja. Za OpenGL compute treba prilagoditi.

## Način rada (testovi ispravnosti)

Svaki korak se provjerava prije sljedećeg:

- `sigma = 0` daje sliku bez magle (`T = 1`, `L_acc = 0`).
- Vrlo gusta magla daje `T → 0`.
- Rezultat froxel metode se poredi s referentnim brute-force ray marchingom (isti `sigma`, `g`, svjetlo).
- Povećanje rezolucije grida mora konvergirati prema referenci.
- Segment s `sigma_t → 0` ne smije dati NaN (zaštita u koraku dijeljenja).
- SIMD i OpenMP verzije moraju dati isti rezultat kao sekvencijalna (do tolerancije `1e-5`).

## Otvorena pitanja

- **Shadow mapa na CPU-u.** Mapa je u GPU memoriji, a injection je na CPU-u. Opcije: (1) readback svaki frame (spor transfer može pokvariti mjerenja), (2) CPU rasterizacija dubinske mape iz svjetla, (3) statična testna scena s mapom iz fajla. Odluka: TODO.
- Potvrditi s profesorom da je CPU (OpenMP/SIMD) traženi cilj, a GPU ide u drugi projekat.

## Rječnik pojmova

- **Participating medium**: medij u zraku (magla, dim, prašina) koji apsorbuje i raspršuje svjetlo.
- **Apsorpcija (sigma_a), raspršenje (sigma_s), ekstinkcija (sigma_t)**: `sigma_t = sigma_a + sigma_s`.
- **Out-scattering / in-scattering**: zraka gubi svjetlo odbijanjem s puta / dobija svjetlo odbijeno prema oku.
- **Beer-Lambert zakon**: svjetlo slabi eksponencijalno s pređenim putem.
- **Transmitansa (T)**: dio svjetla koji preživi put, između 0 i 1.
- **Fazna funkcija, Henyey-Greenstein, g**: raspodjela smjerova raspršenja i njen jedini parametar.
- **Single scattering**: računa se samo jedno raspršenje (svjetlo, čestica, oko).
- **Shadow map**: mapa dubina iz svjetla, služi za vidljivost `V(t)`.
- **God rays**: vidljivi snopovi svjetla zbog sjena u mediju.
- **Ray marching**: koračanje duž zrake uz sabiranje doprinosa.
- **Froxel**: frustum + voxel, ćelija 3D grida poravnatog s kamerom.
- **SoA / AoS**: Structure of Arrays / Array of Structures, raspored podataka u memoriji.
- **OpenMP, SIMD, AVX2, FMA**: paralelizacija po nitima, vektorske instrukcije, 256-bitni skup instrukcija, fused multiply-add.
- **Speedup / efikasnost**: `T_seq / T_par` i `speedup / broj_niti`.
- **Prefix scan** (za GPU projekat): kumulativni zbir. Segmenti `(L, T)` se komponuju asocijativno: `(L1,T1) o (L2,T2) = (L1 + T1*L2, T1*T2)`.
