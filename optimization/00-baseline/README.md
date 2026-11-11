# Sekvencijalni baseline — predložak

Status: nije profilirano. Funkcionalne provjere modularizacije nisu baseline mjerenje.

- Datum / osoba:
- Izvorni kod (hash + lokalni patch):
- OS, CPU, RAM, power plan / napajanje:
- Kompajler, verzija, arhitektura, CMake generator i build opcije:
- Workload: `window-room-v1`, rezolucija, koraci, sjene / shadow rezolucija:
- Komanda, warmup, iteracije, broj nezavisnih pokretanja:
- Checksum i rezultat validacije:
- AMD uProf verzija, profil / sampling period, dostupni brojači:
- Putanje do EXE, DLL, simbola i sirovog profiler sessiona:
- Zapažanja / hotspots:

Primjer iz korijena repozitorija nakon CPU builda:

```powershell
.\build-cpu\volumetrics\VolumetricBenchmark.exe --iterations 1000 --warmup 3 --width 240 --height 180 --steps 64 --shadow-size 1024 --csv optimization/00-baseline/timings.csv
```
