# V2 — inline `Visibility` i Intel VTune mikroarhitekturna analiza

## 1. Šta je promijenjeno

V2 u odnosu na [V1](../v1/README.md) mijenja samo jednu stvar:
`__forceinline` (samo za MSVC) na funkciji `Visibility` u
[Lighting.cpp](../../volumetrics/src/Lighting.cpp) (oko linije 65). Algoritam, parametri i
rezultat su isti, checksum svih iteracija je identičan (`251039.4544048`).
Na GCC-u izmjena nema efekta, pa se V2 mjeri samo MSVC buildom.

## 2. Mjerenja vremena (benchmark, bez profilera)

Pokretanja na drugoj mašini (Windows 11, MSVC), po 1.000 iteracija, isti workload
(`window-room-v1`, 240 × 180, 64 koraka, shadow mape 1024²). CSV-ovi su u [results/](results/).

| Serija | Prosjek V1 | Prosjek V2 | Promjena |
| --- | ---: | ---: | ---: |
| 1. mjerenje | 34,159 ms | 27,574 ms | −19,3% |
| 2. mjerenje (ponovljeno) | 33,438 ms | 28,498 ms | −14,8% |

Ove vrijednosti **ne porediti** s V0/V1 brojevima iz [v1/README.md](../v1/README.md)
(Ryzen 7 7730U, GCC 13.1). To je druga mašina i drugi kompajler.

## 3. Intel VTune — Microarchitecture Exploration (Intel i7-10510U)

Mjerenje s hardverskim brojačima na **Intel Core i7-10510U (Comet Lake U), Windows 10,
MSVC Release**. Collector: *Event-based sampling driver*. Isti benchmark, isti parametri
(`--iterations 1000 --warmup 3 --width 240 --height 180 --steps 64 --shadow-size 1024`),
ugašeni ostali programi. VTune Profiler **2024.3**. Novija verzija (2026.6) ne prepoznaje
ovaj procesor (*"VTune Profiler cannot recognize the processor"*), pa je korištena starija.

Samo jedno pokretanje po verziji, pa su vrijednosti indikativne, ne statistika.
Elapsed time uključuje trošak profilera.

| Metrika | V1 | V2 | Promjena |
| --- | ---: | ---: | ---: |
| Elapsed time | 39,077 s | 35,256 s | −9,8% |
| Clockticks | 165,08 mlrd | 146,19 mlrd | −11,4% |
| Instructions retired | 411,37 mlrd | 352,30 mlrd | −14,4% |
| CPI | 0,401 | 0,415 | +3,5% |
| Retiring | 64,3% | 60,1% | |
| Front-End Bound | 0,8% | 0,9% | |
| Bad Speculation | 2,2% | 2,4% | |
| **Back-End Bound** | 32,7% | 36,6% | |
| Memory Bound | 3,2% | 2,6% | |
| **Core Bound** | 29,5% | 34,0% | |
| Vector Capacity Usage (FPU) | 12,6% | 12,6% | |
| Efektivno fizičkih jezgara | 0,980 od 4 | 0,979 od 4 | |

### Funkcija `Visibility` (Bottom-up)

| | V1 | V2 |
| --- | ---: | ---: |
| CPU time | 22,948 s | 20,237 s |
| Instructions retired | 269,36 mlrd | 214,37 mlrd |
| CPI | 0,368 | 0,399 |
| Retiring | 66,2% | 57,0% |

### Zapažanja

- **Memorija nije usko grlo.** Memory Bound je 2,6 do 3,2%. Hipoteza da je `Visibility`
  ograničen pristupom shadow mapama se **nije potvrdila**.
- **Kod je Core Bound**, 29,5% (V1) i 34,0% (V2) slotova. Izvršne jedinice su zasićene
  aritmetikom, uz vrlo malo vektorskih instrukcija (FPU vektorski kapacitet 12,6%).
- **Inline je smanjio broj instrukcija** (−14,4% ukupno, −20,4% u `Visibility`) i vrijeme,
  ali CPI i udio Core Bound su porasli: preostale instrukcije su "skuplje", pa je dobitak
  u vremenu manji od dobitka u broju instrukcija.
- **`glm::operator*`, `glm::operator+`, `glm::vec3` konstruktor i `std::min` i dalje
  postoje kao zasebne funkcije** (nisu inlinane), što se vidi u Bottom-up tabeli za obje
  verzije.
- **Jedna nit:** efektivno se koristi 0,98 od 4 fizička jezgra (Poor), pa je prostor za
  OpenMP velik.

### Sljedeće izmjene (predložene, još nisu izmjerene)

1. Inline ostalih glm operatora i `std::min` u petlji ray marchinga.
2. SIMD (AVX2/FMA) za integraciju i provjeru vidljivosti: cilj je podići vektorski
   kapacitet s 12,6%.
3. OpenMP paralelizacija po redovima piksela (jezgra se trenutno ne koriste).

## 4. Screenshotovi

Intel i7-10510U, VTune 2024.3, Microarchitecture Exploration ([screenshots/](screenshots/)):

| Verzija | Summary | CPU utilization | Collection info | Bottom-up |
| --- | --- | --- | --- | --- |
| V1 | [slika](screenshots/v1-intel-uarch-1-summary.png) | [slika](screenshots/v1-intel-uarch-2-cpu-utilization.png) | [slika](screenshots/v1-intel-uarch-3-collection-info.png) | [slika](screenshots/v1-intel-uarch-4-bottomup.png) |
| V2 | [slika](screenshots/v2-intel-uarch-1-summary.png) | [slika](screenshots/v2-intel-uarch-2-cpu-utilization.png) | [slika](screenshots/v2-intel-uarch-3-collection-info.png) | [slika](screenshots/v2-intel-uarch-4-bottomup.png) |

Raniji screenshotovi `vtune-v1-*.png` i `vtune-v2-*.png` su **Hotspots** analize u
*User-mode sampling* modu s druge mašine, bez hardverskih brojača. Pokazuju samo vrijeme po
funkciji (`Visibility` 67 do 71% CPU vremena).

## 5. Ograničenja

- Jedno pokretanje po verziji u VTuneu, bez ponavljanja.
- Vremena iz VTunea (elapsed) i iz benchmark CSV-ova nisu međusobno uporediva.
- Vremena iz poglavlja 2 su s druge mašine nego VTune mjerenja iz poglavlja 3.
- V1 i V2 su građeni MSVC-om. V1 je izgrađen iz commita `aa00d29`, V2 iz `9c14322`.
