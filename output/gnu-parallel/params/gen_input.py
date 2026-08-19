#!/usr/bin/env python3
"""
Copyright (c) 2026 IDLab (UAntwerp & imec)

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3 as
published by the Free Software Foundation;

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

Author: Douglas D. Agbeve <douglas.agbeve@uantwerpen.be>



Generate the GNU-parallel input CSV for the ambient-IoT scheme sweep.

Columns: run,numUes,scheme   (consumed by ../job_script.sh)
  scheme in {RA, idealfug, hybridsr, dedicated, fug}:
    RA        - Random Access (contention baseline)
    idealfug  - Idealised FUG (connected + cDRX + oracle/ideal BSR, upper bound)
    hybridsr  - Hybrid Scheduling Request (deep-sleep + dedicated SR + contention fallback)
    dedicated - Dedicated Scheduling Request only (deep-sleep + reserved SR, no contention)
    fug       - Proactive FUG, prediction arm (eNB predicts each UE's period, pushes grants)

Each scheme is a single configuration (no hybrid on/off split any more), so
there is exactly one row per (run, numUes, scheme).

Usage:
    python3 gen_input.py [out.csv]        # default: nbiot_input_ambient.csv
Edit RUNS / NUMUES / SCHEMES below to change the sweep.
"""


import sys

RUNS    = range(1, 11)                              # RngRun seeds 1..10
NUMUES  = [1, 20, 40, 60, 80, 100, 120, 140, 160, 180, 200]
SCHEMES = ["RA", "idealfug", "hybridsr", "dedicated", "fug"]
HARVESTS = [0.001, 0.0015, 0.002]             # peak harvest in W

def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "nbiot_input_ambient.csv"
    rows = [f"{run},{n},{scheme},{h}"
            for run in RUNS
            for n in NUMUES
            for scheme in SCHEMES
            for h in HARVESTS]
    with open(out, "w") as f:
        f.write("run,numUes,scheme,harvest\n")
        f.write("\n".join(rows) + "\n")
    print(f"wrote {out}: {len(rows)} job rows "
          f"({len(list(RUNS))} runs x {len(NUMUES)} densities x "
          f"{len(SCHEMES)} schemes x {len(HARVESTS)} harvest levels)")


if __name__ == "__main__":
    main()
