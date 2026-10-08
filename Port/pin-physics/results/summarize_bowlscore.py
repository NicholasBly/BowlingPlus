#!/usr/bin/env python3
"""Summarise the Bowlscore runs of pinlab.

Usage (from this folder):  python3 summarize_bowlscore.py > SUMMARY.md

Reads every bowlscore_<config>.csv next to this script. Each row is one shot:
  cfg, offset_in, entry (degrees), shot, down (pins down after the first ball), leave
A strike is down == 10. The 95 % interval is the Wilson score interval for a proportion.
"""
import csv
import glob
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

LABELS = {
    "stock": "Game as shipped (7.5 ms step, friction 0.5/0.3)",
    "ref": "Converged reference (0.5 ms step, 20 solver iterations)",
    "stock_pf025": "Pin friction 0.25, game's step",
    "stock_pf03": "Pin friction 0.30, game's step",
    "dt375": "Double rate (3.75 ms), game's friction",
    "dt375_pf02": "Double rate, pin friction 0.20",
    "dt375_pf025": "Double rate, pin friction 0.25 (BowlingPlus 1.7.1 setting)",
    "dt375_pf03": "Double rate, pin friction 0.30",
    "dt375_pf035": "Double rate, pin friction 0.35",
    "dt375_pf04": "Double rate, pin friction 0.40",
    "dt375_deck03": "Double rate, deck friction 0.30",
    "dt375_e75": "Double rate, pin restitution 0.75",
}


def wilson(k, n, z=1.959964):
    if n == 0:
        return 0.0, 0.0
    p = k / n
    d = 1 + z * z / n
    c = (p + z * z / (2 * n)) / d
    h = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / d
    return 100 * (c - h), 100 * (c + h)


def load(path):
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def pct(rows):
    n = len(rows)
    k = sum(1 for r in rows if int(r["down"]) == 10)
    lo, hi = wilson(k, n)
    return n, k, 100 * k / n if n else 0.0, (hi - lo) / 2


def main():
    files = sorted(glob.glob(os.path.join(HERE, "bowlscore_*.csv")))
    if not files:
        sys.exit("no bowlscore_*.csv files next to this script")
    print("| Setup | Shots | Strikes | ±95 % | 0-3 degrees | 6-10 degrees |")
    print("|---|---|---|---|---|---|")
    for path in files:
        cfg = os.path.basename(path)[len("bowlscore_"):-len(".csv")]
        rows = load(path)
        if not rows:
            continue
        n, k, p, half = pct(rows)
        low = [r for r in rows if int(r["entry"]) <= 3]
        high = [r for r in rows if int(r["entry"]) >= 6]
        _, _, pl, _ = pct(low)
        _, _, ph, _ = pct(high)
        label = LABELS.get(cfg, cfg)
        print(f"| {label} (`{cfg}`) | {n} | {p:.1f} % | ±{half:.1f} | {pl:.1f} % | {ph:.1f} % |")


if __name__ == "__main__":
    main()
