# Evidencija budućih optimizacija

Ovaj folder je priprema za buduće mjerenje. Baseline profiliranje još nije urađeno.

```text
optimization/
  00-baseline/             izvorna sekvencijalna verzija
    screenshots/          snimci AMD uProf-a
    README.md             metapodaci i komande
    metrics.csv           predložak rezultata
  iterations/             kopirati isti format za 01-..., 02-...
  comparisons/            poređenja s baselineom
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
