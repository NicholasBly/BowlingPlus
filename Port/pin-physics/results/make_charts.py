#!/usr/bin/env python3
"""Draws the study's two charts from the CSVs in this folder (needs matplotlib).

Usage (from this folder):  python3 make_charts.py   ->  ../docs/pin-physics-bowlscore.png, ../docs/pin-physics-pinpin.png
"""
import csv, os, statistics
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE = os.path.dirname(os.path.abspath(__file__))
DOCS = os.path.join(HERE, "..", "docs")


def rows(name):
    with open(os.path.join(HERE, name), newline="") as f:
        return list(csv.DictReader(f))


def strike_pct(r):
    return 100.0 * sum(1 for x in r if int(x["down"]) == 10) / len(r)


def bowlscore():
    setups = [("stock", "Game as shipped", "#c0392b"),
              ("ref", "Same model,\nconverged numerics", "#7f8c8d"),
              ("stock_pf025", "Pin friction 0.25\n(BowlingPlus 1.7.0-1.7.4)", "#8e44ad"),
              ("stock_pf04", "Pin friction 0.40\n(1.7.5, game's step)", "#2e86c1"),
              ("dt375_pf025", "2x physics rate +\npin friction 0.25", "#27ae60")]
    fig, (a, b) = plt.subplots(1, 2, figsize=(15, 5.2))
    vals = [strike_pct(rows("bowlscore_%s.csv" % k)) for k, _, _ in setups]
    a.axhspan(41.9, 44.3, color="#f7dc6f", alpha=0.6, label="Real pins, USBC Bowlscore (41.9-44.3%)")
    bars = a.bar(range(len(setups)), vals, color=[c for _, _, c in setups])
    for i, v in enumerate(vals): a.text(i, v + 0.8, "%.1f%%" % v, ha="center", fontsize=11)
    a.set_xticks(range(len(setups)))
    a.set_xticklabels([n for _, n, _ in setups], fontsize=9)
    a.set_ylim(0, 60)
    a.set_ylabel("Strike % over 23 offsets x 11 entry angles (1,012 shots, +-3)")
    a.set_title("Bowlscore rebuilt: how often each setup strikes")
    a.legend(loc="upper left", fontsize=9)
    for k, n, c in setups:
        r = rows("bowlscore_%s.csv" % k)
        xs, ys = [], []
        for lo in range(0, 11, 2):
            band = [x for x in r if lo <= int(x["entry"]) <= lo + 1]
            xs.append(lo + 0.5); ys.append(strike_pct(band))
        b.plot(xs, ys, "o-", color=c, label=n.replace("\n", " "))
    b.axhspan(41.9, 44.3, color="#f7dc6f", alpha=0.4)
    b.set_ylim(0, 70); b.set_xlim(0, 11)
    b.set_xlabel("Entry angle (degrees, in 2-degree bands)")
    b.set_ylabel("Strike %")
    b.set_title("Strike % by entry angle")
    b.legend(fontsize=8, loc="lower left")
    fig.tight_layout()
    fig.savefig(os.path.join(DOCS, "pin-physics-bowlscore.png"), dpi=100)


def pinpin():
    r = rows("pinpin.csv")
    key = lambda x: (x["kind"], x["v"], x["offset"], x["spin"])
    ref = {key(x): x for x in r if x["cfg"] == "ref"}
    setups = [("stock", "Game (7.5 ms)"), ("co10", "Contact offset 10 mm"), ("it20", "20 solver iterations"),
              ("spec", "Speculative CCD on pins"), ("dt375", "3.75 ms step"), ("dt25", "2.5 ms step")]
    err, flips = [], []
    for k, _ in setups:
        rs = [x for x in r if x["cfg"] == k]
        e = [abs(float(x["tSpeed"]) - float(ref[key(x)]["tSpeed"])) / float(ref[key(x)]["tSpeed"]) * 100
             for x in rs if float(ref[key(x)]["tSpeed"]) > 0.3]
        err.append(statistics.mean(e))
        flips.append(sum(1 for x in rs if x["knocked"] != ref[key(x)]["knocked"]))
    fig, ax = plt.subplots(figsize=(10.5, 4.6))
    xs = range(len(setups))
    ax.bar([x - 0.2 for x in xs], err, 0.4, color="#2e86c1", label="Struck-pin speed error vs converged (%)")
    ax.bar([x + 0.2 for x in xs], flips, 0.4, color="#c0392b", label="Knocked-over outcome flips (of 360 hits)")
    ax.set_xticks(list(xs)); ax.set_xticklabels([n for _, n in setups], fontsize=9)
    ax.set_title("Pin-on-pin hits: what each engine setting does")
    ax.legend(fontsize=9)
    fig.tight_layout()
    fig.savefig(os.path.join(DOCS, "pin-physics-pinpin.png"), dpi=100)


if __name__ == "__main__":
    os.makedirs(DOCS, exist_ok=True)
    bowlscore()
    pinpin()
    print("wrote ../docs/pin-physics-bowlscore.png and ../docs/pin-physics-pinpin.png")
