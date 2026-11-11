# Evidencija profiliranja i budućih optimizacija

Završena je analiza **Baseline V0**, početnog sekvencijalnog CPU algoritma:
[izvještaj, originalni CSV-ovi, screenshotovi i grafikoni](baseline/README.md).
Dokumentovanje nije promijenilo CPU/GPU kod, benchmark parametre ni mjerenja.

```text
optimization/
  baseline/               stvarni V0 podaci i analiza: pet serija × 1.000 mjerenja
    results/              originalni CSV-ovi i izvedene statistike
    screenshots/          originalne uProf slike
    charts/               grafikoni izvedeni iz podataka
    metadata/             konfiguracija, porijeklo i hash vrijednosti
    profiler/             zasebna vremena profiliranog pokretanja
  00-baseline/            raniji prazan predložak, nije druga serija mjerenja
  iterations/             buduće iteracije: 01-..., 02-...
  comparisons/            buduća poređenja s V0
```

Za svaku iteraciju sačuvati stanje izvornog koda (commit/hash ili patch ako nije
commitano), CMakeCache/kompajler i opcije, EXE + DLL i pripadajuće simbole,
tačnu benchmark komandu, checksum, CSV vremena, sirovi profiler session/export
i screenshotove. Ne mijenjati workload, parametre ili toolchain između verzija
koje se direktno porede. Ubrzanje računati kao `baseline_ms / iteration_ms`.

Mjeriti isti broj zagrijavanja i ponavljanja. Za poređenje vremena koristiti više
samostalnih pokretanja bez profilera i bez drugog CPU opterećenja. Zabilježiti
napajanje laptopa, power plan, temperaturu i eventualni affinity. Profiliranje
služi analizi, a samo po sebi dodaje trošak mjerenju.

Detalji builda, simbola i pripreme za AMD uProf su u
[uputama modula](../volumetrics/README.md). Nedostupne hardverske brojače označiti
`N/A`; ne tumačiti procijenjeni throughput kao broj izvršenih instrukcija.

Raniji `00-baseline/` ostaje predložak radi postojećih linkova. Referentni podaci
su u `baseline/`. Ne prepisivati ih novim mjerenjima. Inclusive/exclusive uzorke
profilera ne sabirati s benchmark intervalima.
