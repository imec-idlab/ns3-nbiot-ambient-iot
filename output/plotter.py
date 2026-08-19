#!/usr/bin/env python3
"""Plotter for the NB-IoT ambient-IoT sweep results.

Reads the per-run outputs produced by job_script.sh, laid out as

    <base>/output_ambient_h<harvestW>/<scheme>/<run>_run-<nUe>_Ues/

aggregates them to mean +/- std across seeds, and writes the paper figures
(PDF) into <base>/fig/:

    fig/ambient_h0.002/delay_mean.pdf                mean UL delay
    fig/ambient_h0.002/pdr_deadline_10s.pdf          deadline-PDR (10 s)
    fig/ambient_h0.002/energy_per_bit_comparison.pdf energy per delivered bit
    fig/stress/outofenergy_count.pdf                 brown-out fraction vs
                                                     density and harvest peak

Usage:
    python plotter.py            # run from the directory holding the outputs
    python plotter.py --base /path/to/outputs
"""
import argparse
from pathlib import Path

import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D

plt.rcParams.update({
    "figure.figsize": (4.0, 3.1),
    "font.size": 12,
    "axes.labelsize": 12,
    "xtick.labelsize": 11,
    "ytick.labelsize": 11,
    "legend.fontsize": 10,
    "pdf.fonttype": 42,
})

# ======================= CONFIG =======================
HARVEST_ROOTS = {          # peak harvest mW -> output root (relative to --base)
    1.0: "output_ambient_h0.001",
    1.5: "output_ambient_h0.0015",
    2.0: "output_ambient_h0.002",
}
OPERATING_MW = 2.0                   # harvest peak for the KPI figures
STRESS_NUE   = [40, 120, 200]        # densities in the harvest-sweep figure
P_STYLES     = {1.0: "-", 1.5: "--", 2.0: ":"}   # linestyle per harvest level

# scheme dir-name -> (display label, colour, marker, linestyle)
SCHEMES = {
    "RA":        ("RA",              "tab:blue",   "o", "-"),
    "fug":       ("FUG",             "tab:green",  "^", "-"),
    "idealfug":  ("Idealized FUG",   "tab:purple", "D", "--"),
    "dedicated": ("SR",              "tab:orange", "v", "-"),
    "hybridsr":  ("Hybrid SR",       "tab:red",    "s", "-"),
}

RUNS       = list(range(1, 11))
NUE_VALUES = [1, 20, 40, 60, 80, 100, 120, 140, 160, 180, 200]

DEADLINE_MS        = 10_000.0   # packets later than this count as lost
APP_PAYLOAD_BYTES  = 49
MIN_RUNS_PER_POINT = 1

TARGET_STATES = [
    "RRC_CONNECTED_SENDING_NPRACH",
    "RRC_CONNECTED_SENDING_NPUSCH",
    "RRC_CONNECTED_SENDING_NPUSCH_F2",
    "RRC_CONNECTED_RECEIVING_NPDCCH",
    "RRC_CONNECTED_RECEIVING_NPDSCH",
    "RRC_CONNECTED_IDLE",
    "RRC_SUSPENDED_EDRX",
    "RRC_SUSPENDED_DRX",
    "RRC_SUSPENDED_PSM",
]

# summary.out is written only at successful teardown, so its presence is the
# completed-run check (walltime-killed jobs are missing it).
REQUIRED_FILES = (
    "energy_states.out", "rxbytes.out", "summary.out",
    "energy_per_ue.out", "delays.out",
)
# ======================================================


# ---------------- COLLECTION ----------------
def collect_data(base_path, label=""):
    """One row per (run, nUe), aggregated to mean +/- std across runs at each
    nUe. Metrics: mean delay, deadline-PDR, energy per bit, depletion."""
    all_rows = []

    for run in RUNS:
        for nUe in NUE_VALUES:
            run_dir = base_path / f"{run}_run-{nUe}_Ues"

            missing = [f for f in REQUIRED_FILES if not (run_dir / f).exists()]
            if missing:
                print(f"[{label}] skip run={run} nUe={nUe}: missing {missing}")
                continue

            # Windowed per-state energy (exported in-model at statsEnd)
            try:
                df_states = pd.read_csv(run_dir / "energy_states.out", delimiter="\t")
            except Exception as e:
                print(f"[{label}] skip run={run} nUe={nUe}: energy-states-read {e}")
                continue
            st_col = df_states["State"].astype(str).str.strip()
            en_col = pd.to_numeric(df_states["Energy_J"], errors="coerce")
            total_energy = en_col.loc[st_col.isin(TARGET_STATES)].sum()

            # Per-packet delays (deadline accounting)
            n_within_deadline = float("nan")
            try:
                df_del = pd.read_csv(run_dir / "delays.out", delimiter="\t")
                dd = pd.to_numeric(df_del["Delay_ms"], errors="coerce").dropna()
                if len(dd):
                    n_within_deadline = int((dd <= DEADLINE_MS).sum())
            except Exception as e:
                print(f"[{label}] warn run={run} nUe={nUe}: delays-read {e}")

            # summary.out: app metrics + depletion
            n_depleted = 0
            app_delay = app_sent = app_rx = float("nan")
            try:
                s = pd.read_csv(run_dir / "summary.out", delimiter="\t")
                n_depleted = int(s["nDepleted"].iloc[0])
                app_delay = float(s["appMeanDelay_ms"].iloc[0])
                app_sent  = float(s["appSent"].iloc[0])
                app_rx    = float(s["appReceived"].iloc[0])
            except Exception as e:
                print(f"[{label}] warn run={run} nUe={nUe}: summary-read {e}")

            agg_rx_pkts = 0 if pd.isna(app_rx) else int(app_rx)
            agg_tx_pkts = 0 if pd.isna(app_sent) else int(app_sent)
            pdr_deadline = (n_within_deadline / agg_tx_pkts) \
                if (agg_tx_pkts and not pd.isna(n_within_deadline)) else float("nan")
            delivered_bits = agg_rx_pkts * APP_PAYLOAD_BYTES * 8
            eperbit_mJ = (total_energy / delivered_bits * 1e3) \
                if delivered_bits > 0 else float("nan")

            all_rows.append({
                "Run":               run,
                "nUe":               nUe,
                "delay_mean_ms":     app_delay,
                "pdr_deadline":      pdr_deadline,
                "energy_per_bit_mJ": eperbit_mJ,
                "frac_depleted":     (n_depleted / nUe) if nUe else float("nan"),
            })

    df = pd.DataFrame(all_rows)
    if df.empty:
        return None

    metric_cols = ["delay_mean_ms", "pdr_deadline", "energy_per_bit_mJ",
                   "frac_depleted"]
    agg_dict = {"n_runs": ("Run", "nunique")}
    for c in metric_cols:
        agg_dict[c]           = (c, "mean")
        agg_dict[c + "__std"] = (c, "std")

    df_sum = (df.groupby("nUe")
                .agg(**agg_dict)
                .reindex(NUE_VALUES)
                .reset_index())

    low_conf = df_sum["n_runs"].fillna(0) < MIN_RUNS_PER_POINT
    for c in df_sum.columns:
        if c not in ("nUe", "n_runs"):
            df_sum.loc[low_conf, c] = pd.NA

    missing = df_sum.loc[df_sum["n_runs"].isna() | low_conf, "nUe"].tolist()
    if missing:
        print(f"[{label}] missing/low-confidence nUe: {missing}")

    return df_sum


# ---------------- PLOT HELPERS ----------------
def plot_metric(results, arms, col, ylabel, outfile, with_err=True, logy=False,
                yticks=None, ycap=None):
    outfile = Path(outfile).with_suffix(".pdf")
    plt.figure()
    for label, df in results.items():
        if df is None:
            print(f"{label}: no data for {col}")
            continue
        st = arms[label]
        err_col = col + "__std"
        if with_err and err_col in df.columns:
            lo = np.minimum(df[err_col].fillna(0), df[col].fillna(0))  # floor 0
            hi = df[err_col].fillna(0)
            if ycap is not None:                     # ceiling for ratio metrics
                hi = np.minimum(hi, np.maximum(ycap - df[col].fillna(0), 0))
            plt.errorbar(df["nUe"], df[col], yerr=[lo, hi],
                         color=st["color"], linestyle=st["linestyle"],
                         marker=st["marker"], capsize=3, label=label)
        else:
            plt.plot(df["nUe"], df[col],
                     color=st["color"], linestyle=st["linestyle"],
                     marker=st["marker"], label=label)
    if logy:
        plt.yscale("log")
    if yticks is not None:
        plt.yticks(yticks)
    plt.xlabel("Number of UEs")
    plt.ylabel(ylabel)
    plt.xticks(NUE_VALUES[::2])
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.savefig(outfile, dpi=300, bbox_inches="tight", pad_inches=0.02)
    plt.close()
    print(f"wrote {outfile}")


def plot_depleted_vs_harvest(roots, outfile, nues=STRESS_NUE):
    """frac_depleted vs nUe; color = scheme, linestyle + marker size = harvest
    peak (larger markers = lower harvest). Mean over seeds; summary.out only.
    fug excluded (prediction-driven brown-outs, different mechanism)."""
    outfile = Path(outfile).with_suffix(".pdf")
    p_ms = {1.0: 7.5, 1.5: 5.5, 2.0: 4.0}          # marker-size ramp
    plt.figure(figsize=(5.3, 3.1))
    for scheme, (disp, c, m, _) in SCHEMES.items():
        if scheme == "fug":
            continue
        for p_mw, root in roots.items():
            ys = []
            for n in nues:
                vals = []
                for run in RUNS:
                    f = root / scheme / f"{run}_run-{n}_Ues" / "summary.out"
                    if f.exists():
                        vals.append(int(pd.read_csv(f, delimiter="\t")
                                        ["nDepleted"].iloc[0]) / n)
                ys.append(np.mean(vals) if vals else float("nan"))
            plt.plot(nues, ys, color=c, marker=m,
                     linestyle=P_STYLES[p_mw], markersize=p_ms[p_mw])
    handles = ([Line2D([], [], color=c, marker=m, linestyle="-", label=disp)
                for sch, (disp, c, m, _) in SCHEMES.items() if sch != "fug"]
               + [Line2D([], [], color="black", linestyle=ls,
                         label=f"$P_h^{{max}}$ = {p:g} mW")
                  for p, ls in P_STYLES.items()])
    plt.legend(handles=handles, loc="center left", bbox_to_anchor=(0.95, 0.5),
               fontsize=8.5, framealpha=0.9, handlelength=2.6,
               labelspacing=0.4)
    plt.xlabel("Number of UEs")
    plt.ylabel("Fraction of depleted UEs")
    plt.xticks(nues)
    plt.ylim(bottom=0)
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(outfile, dpi=300, bbox_inches="tight", pad_inches=0.02)
    plt.close()
    print(f"wrote {outfile}")


# ---------------- MAIN ----------------
def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--base", type=Path, default=Path("."),
                    help="directory containing the output_ambient_h* roots "
                         "(default: current directory)")
    args = ap.parse_args()

    roots = {p: args.base / r for p, r in HARVEST_ROOTS.items()}
    op_tag = HARVEST_ROOTS[OPERATING_MW].replace("output_ambient_", "")
    figdir_kpi    = args.base / "fig" / f"ambient_{op_tag}"
    figdir_stress = args.base / "fig" / "stress"
    figdir_kpi.mkdir(parents=True, exist_ok=True)
    figdir_stress.mkdir(parents=True, exist_ok=True)

    # KPI figures at the operating point
    arms = {disp: {"path": roots[OPERATING_MW] / scheme,
                   "color": c, "marker": m, "linestyle": ls}
            for scheme, (disp, c, m, ls) in SCHEMES.items()}
    results = {label: collect_data(arm["path"], f"{label}@{OPERATING_MW:g}mW")
               for label, arm in arms.items()}
    # fug appears ONLY in the deadline-PDR figure.
    arms_nofug    = {k: v for k, v in arms.items()    if k != "FUG"}
    results_nofug = {k: v for k, v in results.items() if k != "FUG"}

    plot_metric(results_nofug, arms_nofug, "delay_mean_ms",
                "Average UL delay (ms)", figdir_kpi / "delay_mean")
    plot_metric(results, arms, "pdr_deadline",
                "Packet Delivery Ratio", figdir_kpi / "pdr_deadline_10s",
                ycap=1.0)
    plot_metric(results_nofug, arms_nofug, "energy_per_bit_mJ",
                "Energy per bit (mJ/bit)",
                figdir_kpi / "energy_per_bit_comparison")

    # Brown-out fraction vs density and harvest peak
    plot_depleted_vs_harvest(roots, figdir_stress / "outofenergy_count")


if __name__ == "__main__":
    main()
