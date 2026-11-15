"""Recompute V0/V1 comparison from existing CSVs; never run the benchmark."""
import csv
import hashlib
import json
import math
from pathlib import Path
import statistics

BASE = Path(__file__).resolve().parent
ROOT = BASE.parents[1]
FIELDS = ["iteration", "kernel_ms", "checksum", "width", "height", "steps",
          "shadow_size", "shadows", "density", "albedo", "g", "max_distance"]


def describe(values):
    return dict(n=len(values), mean_ms=statistics.mean(values),
                median_ms=statistics.median(values), min_ms=min(values),
                max_ms=max(values), sample_sd_ms=statistics.stdev(values))


def load_version(version, folder, prefix):
    runs, pooled = [], []
    parameters = checksum = None
    for i in range(1, 6):
        path = folder / f"{prefix}_{i:02}.csv"
        with path.open(encoding="utf-8-sig", newline="") as stream:
            reader = csv.DictReader(stream)
            if reader.fieldnames != FIELDS:
                raise ValueError(f"Unexpected CSV schema: {path}")
            rows = list(reader)
        if len(rows) != 1000 or [int(r["iteration"]) for r in rows] != list(range(1, 1001)):
            raise ValueError(f"Expected 1000 ordered iterations: {path}")
        times = [float(r["kernel_ms"]) for r in rows]
        if not all(math.isfinite(x) and x > 0 for x in times):
            raise ValueError(f"Invalid timing: {path}")
        for row in rows:
            params = {key: row[key] for key in FIELDS[3:]}
            if parameters is None:
                parameters, checksum = params, row["checksum"]
            if params != parameters or row["checksum"] != checksum:
                raise ValueError(f"Changing parameters or checksum: {path}")
        runs.append(dict(version=version, run=path.stem, **describe(times)))
        pooled.extend(times)
    return dict(version=version, **describe(pooled), runs=runs,
                parameters_raw_csv=parameters, checksum=checksum)


def chart(v0, v1):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.ticker import FuncFormatter

    plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 11,
                         "axes.spines.top": False, "axes.spines.right": False,
                         "axes.spines.left": False, "savefig.facecolor": "white"})
    fig, axes = plt.subplots(1, 2, figsize=(10.5, 3.8))
    colors = ["#59697B", "#087F8C"]
    for ax, metric, title in zip(axes, ["mean_ms", "median_ms"], ["Prosječno vrijeme", "Medijan"]):
        values = [v0[metric], v1[metric]]
        ax.barh([0, 1], values, color=colors, height=.52, zorder=2)
        for y, version in enumerate([v0, v1]):
            centres = [run[metric] for run in version["runs"]]
            ax.scatter(centres, [y + (i - 2) * .045 for i in range(5)], s=15,
                       color="white", edgecolor="#1F2933", linewidth=.65, zorder=3)
            ax.text(1.2, y, f"{values[y]:.2f} ms".replace(".", ","),
                    va="center", fontsize=11, weight="bold", color="white")
        decrease = 100 * (1 - values[1] / values[0])
        ax.set_title(f"{title}\n{decrease:.2f}% manje vremena".replace(".", ","),
                     loc="left", fontsize=12, pad=15)
        ax.set_yticks([0, 1], ["V0", "V1"])
        ax.set_ylim(1.55, -.55)
        ax.set_xlim(0, 42)
        ax.set_xticks([0, 10, 20, 30, 40])
        ax.xaxis.set_major_formatter(FuncFormatter(lambda x, _: f"{x:g}"))
        ax.set_xlabel("Vrijeme CPU integracije [ms]")
        ax.tick_params(axis="y", length=0)
        ax.grid(axis="x", alpha=.18, zorder=0)
        ax.set_axisbelow(True)
    fig.suptitle("V0 → V1: sekvencijalna CPU integracija", x=.07, ha="left",
                 fontsize=16, weight="bold", color="#183B4E", y=.98)
    fig.text(.07, .87, "Bez profilera · po 5 × 1.000 integracija · 240 × 180 · 64 koraka · iste shadow mape",
             fontsize=9.5, color="#52616B")
    fig.text(.07, .025, "Trake: statistika svih 5.000 vrijednosti. Tačke: ista statistika svakog od pet pokretanja.",
             fontsize=9, color="#52616B")
    fig.subplots_adjust(left=.07, right=.98, bottom=.20, top=.67, wspace=.31)
    folder = BASE / "charts"
    folder.mkdir(exist_ok=True)
    fig.savefig(folder / "v0-v1-comparison.png", dpi=220)
    fig.savefig(folder / "v0-v1-comparison.svg")
    plt.close(fig)


def main():
    manifest = json.loads((BASE / "metadata/artifacts.json").read_text(encoding="utf-8"))
    for entry in manifest:
        if hashlib.sha256((BASE / entry["archived"]).read_bytes()).hexdigest() != entry["sha256"]:
            raise ValueError(f"Archived evidence changed: {entry['archived']}")
    v0 = load_version("V0", ROOT / "optimization/baseline/results", "baseline")
    v1 = load_version("V1", BASE / "results", "v1")
    if v0["parameters_raw_csv"] != v1["parameters_raw_csv"] or v0["checksum"] != v1["checksum"]:
        raise ValueError("V0/V1 parameters or checksum differ")
    comparison = {metric: dict(reduction_percent=100 * (1 - v1[metric] / v0[metric]),
                               speedup=v0[metric] / v1[metric])
                  for metric in ["mean_ms", "median_ms"]}
    summary = dict(v0=v0, v1=v1, comparison=comparison, filtering="none",
                   aggregation="pool all 5000 measured intervals per version; not median of five medians")
    (BASE / "results/comparison.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    stats = v0["runs"] + v1["runs"]
    with (BASE / "results/run-statistics.csv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(stats[0]))
        writer.writeheader()
        writer.writerows(stats)
    chart(v0, v1)
    print(f"V0: mean={v0['mean_ms']:.8f} ms, median={v0['median_ms']:.8f} ms")
    print(f"V1: mean={v1['mean_ms']:.8f} ms, median={v1['median_ms']:.8f} ms")
    print("Comparison:", json.dumps(comparison))
    print("PASS: 10 x 1000 rows, same parameters/checksum, original CSVs not modified")


if __name__ == "__main__":
    main()
