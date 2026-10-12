#!/usr/bin/env python3
"""Ramps & obstacles (BowlingPlus 1.7.5): the lane layouts, checked, and written to src/RampLayouts.h.

Usage (from the repo root):
  python3 tools/dev/ramps/make_ramps.py [--pinlab PATH] [--check-only]

Every ramp is simulated in pinlab (tools/dev/pinlab, `ramp` experiment: the game's lane, gutters and ball on
PhysX 4.1) at 5.5, 7, 8.5 and 10 m/s (about 12 to 22 mph), aimed at its centre and 5 cm inside each edge. A ramp
passes when, every time, the ball climbs it, stays on the lane, comes down by 16.9 m (well before the head pin at
18.26 m) and is rolling on the lane at 17.6 m. The ramps are "oiled" (no friction, as in the game since 1.7.5's second
build), so a hooking ball goes straight over them. Barstool layouts are checked on paper: every row of stools leaves a
gap a ball fits through, and some straight roll (ball centre within 0.36 m of the lane's centre) misses every stool by
5 cm or more.

Coordinates are the game's: x across the lane from its centre (+x is the 2-pin side, so a right-hander's pocket
is at -x), y down the lane from the foul line, in metres. A ramp is (x, y0, w, h, deg): its front edge at y0,
width w, back edge h above the lane, rising at deg degrees. A stool is (x, y).
"""
import argparse, math, os, struct, subprocess, sys, zlib

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..', '..'))

RAMP_LAYOUTS = [
    ("Kicker",            [(0.00, 6.0, 0.45, 0.15, 12)]),
    ("Big air",           [(0.00, 6.5, 0.50, 0.28, 18)]),
    ("Wall to wall",      [(0.00, 8.0, 1.00, 0.14, 10)]),
    ("Speed bump",        [(0.00, 5.0, 0.70, 0.08, 8)]),
    ("Right kicker",      [(-0.24, 6.0, 0.36, 0.18, 14)]),
    ("Left kicker",       [(0.24, 6.0, 0.36, 0.18, 14)]),
    ("Right edge",        [(-0.34, 5.0, 0.30, 0.12, 12)]),
    ("Left edge",         [(0.34, 5.0, 0.30, 0.12, 12)]),
    ("The needle",        [(0.00, 9.0, 0.26, 0.14, 14)]),
    ("Early launch",      [(0.00, 3.0, 0.45, 0.24, 16)]),
    ("Late hop",          [(0.00, 11.0, 0.45, 0.12, 10)]),
    ("At the arrows",     [(-0.08, 4.0, 0.32, 0.14, 12)]),
    ("At the dots",       [(0.05, 1.9, 0.40, 0.12, 10)]),
    ("Twin ramps",        [(-0.27, 7.0, 0.30, 0.18, 14), (0.27, 7.0, 0.30, 0.18, 14)]),
    ("Staggered pair",    [(-0.26, 5.0, 0.30, 0.15, 12), (0.26, 8.5, 0.30, 0.15, 12)]),
    ("Pick a lane",       [(-0.34, 7.0, 0.26, 0.12, 12), (0.00, 7.0, 0.26, 0.12, 12), (0.34, 7.0, 0.26, 0.12, 12)]),
    ("Short and steep",   [(0.00, 7.0, 0.42, 0.20, 22)]),
    ("Long and gentle",   [(0.00, 6.0, 0.42, 0.20, 8)]),
    ("Right launch pad",  [(-0.20, 8.0, 0.40, 0.26, 18)]),
    ("Left launch pad",   [(0.20, 8.0, 0.40, 0.26, 18)]),
    ("Right needle",      [(-0.16, 10.0, 0.27, 0.12, 12)]),
    ("Left needle",       [(0.16, 10.0, 0.27, 0.12, 12)]),
    ("Wide right",        [(-0.18, 4.0, 0.56, 0.12, 10)]),
    ("Wide left",         [(0.18, 4.0, 0.56, 0.12, 10)]),
    ("Split the gap",     [(-0.24, 9.0, 0.26, 0.15, 14), (0.24, 9.0, 0.26, 0.15, 14)]),
    ("Mega ramp",         [(0.00, 5.5, 0.60, 0.32, 20)]),
    ("Zig-zag",           [(-0.25, 4.0, 0.28, 0.12, 12), (0.25, 7.5, 0.28, 0.12, 12)]),
    ("Breakpoint",        [(-0.30, 11.5, 0.28, 0.10, 10)]),
]

STOOL_LAYOUTS = [
    ("Lone stool",        [(0.00, 9.0)]),
    ("Stool on the right", [(-0.24, 8.0)]),
    ("Stool on the left", [(0.24, 8.0)]),
    ("Squeeze",           [(-0.34, 7.0), (0.34, 7.0)]),
    ("Slalom",            [(-0.21, 5.0), (0.34, 9.0), (0.21, 13.0)]),
    ("The gate",          [(-0.33, 10.0), (0.33, 10.0)]),
    ("Centre pair",       [(0.00, 6.0), (0.00, 11.0)]),
    ("Right wall",        [(-0.36, 5.0), (-0.36, 8.5), (-0.36, 12.0)]),
    ("Left wall",         [(0.36, 5.0), (0.36, 8.5), (0.36, 12.0)]),
    ("Diagonal",          [(-0.34, 4.0), (-0.26, 8.0), (-0.18, 12.0), (-0.10, 16.0)]),
    ("Triangle",          [(0.32, 6.0), (-0.35, 11.0), (0.35, 11.0)]),
    ("Back gate",         [(-0.33, 14.5), (0.33, 14.5)]),
    ("Offset pair",       [(-0.28, 7.0), (0.22, 13.0)]),
    ("Late stool",        [(0.05, 15.5)]),
]

LANE_HALF = 0.527      # the lane surface's half width (RoadPhysMaterial box)
STOOL_R = 0.15         # stool base radius
BALL_R = 0.108
MIN_GAP = 0.27         # a ball (0.216 m) plus room


LINE_MARGIN = 0.05     # a straight roll must miss every stool by this much (beyond touching)
PLAY_HALF = 0.36       # ...with the ball's centre within 0.36 m of the lane's centre (about boards 5 to 35), not along the gutter


def stool_line_clearance(st):
    # The best straight roll from the foul line to the pins (a ball centre that stays on the lane): how far it misses the
    # nearest stool, beyond touching it (stool base radius + ball radius). Rows alone weren't enough: four stools in a
    # diagonal each left a gap in their own row, but no ball could get through all four (1.7.5's first build).
    lim = PLAY_HALF
    xs = [i * 0.005 - lim for i in range(int(2 * lim / 0.005) + 1)]
    best, arg = -9.0, None
    for x0 in xs:
        for x1 in xs:
            c = min(abs(x0 + (x1 - x0) * sy / 18.0 - sx) for sx, sy in st) - (STOOL_R + BALL_R)
            if c > best: best, arg = c, (x0, x1)
    return best, arg


def check_stools():
    bad = []
    for name, st in STOOL_LAYOUTS:
        if len(st) > 4: bad.append((name, "more than 4 stools"))
        for (x, y) in st:
            if not (1.5 <= y <= 16.0): bad.append((name, "stool at y %.1f outside 1.5..16" % y))
            if abs(x) + STOOL_R > LANE_HALF: bad.append((name, "stool off the lane"))
        rows = []
        for s in sorted(st, key=lambda s: s[1]):
            if rows and abs(rows[-1][-1][1] - s[1]) < 0.6: rows[-1].append(s)
            else: rows.append([s])
        for row in rows:
            blocks = sorted((x - STOOL_R, x + STOOL_R) for x, _ in row)
            edge, best = -LANE_HALF, 0.0
            for a, b in blocks:
                best = max(best, a - edge); edge = max(edge, b)
            best = max(best, LANE_HALF - edge)
            if best < MIN_GAP: bad.append((name, "row at y %.1f: widest gap %.2f m" % (row[0][1], best)))
        c, line = stool_line_clearance(st)
        print("%-18s straight roll misses by %.3f m (x %.2f at the foul line to %.2f at the pins)" % (name, c, line[0], line[1]))
        if c < LINE_MARGIN: bad.append((name, "no straight roll gets past all the stools (best misses by %.3f m)" % c))
    return bad


def check_ramps(pinlab):
    specs, idx = [], {}
    for li, (name, ramps) in enumerate(RAMP_LAYOUTS):
        if not (1 <= len(ramps) <= 3): raise SystemExit("%s: 1 to 3 ramps" % name)
        for ri, (x, y0, w, h, deg) in enumerate(ramps):
            if abs(x) + w / 2 > LANE_HALF + 1e-6: raise SystemExit("%s: ramp off the lane" % name)
            rid = li * 10 + ri
            idx[rid] = (name, ri)
            specs.append("%d,%.3f,%.3f,%.3f,%.3f,%.3f" % (rid, x, y0, w, h, deg))
    results = {}
    for rid_spec in specs:
        rid = int(rid_spec.split(',')[0])
        w = float(rid_spec.split(',')[3])
        aims = "0,%.3f,%.3f" % (-(w / 2 - 0.05), w / 2 - 0.05)
        env = dict(os.environ, PINLAB_RAMPS=rid_spec, PINLAB_SPEEDS="5.5,7,8.5,10", PINLAB_AIM=aims, PINLAB_DAMPER="1")
        out = subprocess.run([pinlab, "ramp", os.path.join(REPO, "tools/dev/pinlab/scene.txt"), "dt375"], env=env,
                             capture_output=True, text=True, check=True).stdout.strip().splitlines()
        import csv
        for r in csv.DictReader(out):
            results.setdefault(rid, []).append(r)
    bad, lines = [], []
    for rid, rows in sorted(results.items()):
        name, ri = idx[rid]
        worst_land = max(float(r['landY']) for r in rows)
        fails = []
        for r in rows:
            why = []
            if r['climbed'] != '1': why.append("didn't climb")
            if r['onLane'] != '1': why.append("left the lane")
            if float(r['landY']) < 0 or float(r['landY']) > 16.9: why.append("came down at %s" % r['landY'])
            if r['reached'] != '1' or float(r['zAtPins']) > 0.14: why.append("not rolling at the pins (z %s)" % r['zAtPins'])
            if why: fails.append("v %s aim %s: %s" % (r['v'], r['aim'], ", ".join(why)))
        apex = max(float(r['apex']) for r in rows)
        lines.append("%-18s ramp %d: lands by %.2f m, highest ball centre %.2f m%s" % (name, ri + 1, worst_land, apex, "" if not fails else "  FAIL"))
        for f in fails: bad.append((name, "ramp %d %s" % (ri + 1, f)))
    return bad, lines


def png(w, h, px):
    raw = b''.join(b'\x00' + bytes(sum((px(x, y) for x in range(w)), ())) for y in range(h))
    def chunk(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')


def textures():
    orange, cream = (255, 122, 26, 90), (255, 236, 200, 90)        # alpha: how much of the pin shader's reflection shows
    ramp = png(32, 32, lambda x, y: orange if ((x + y) // 8) % 2 == 0 else cream)
    seat = png(8, 8, lambda x, y: (196, 28, 40, 60) if 1 <= x <= 6 and 1 <= y <= 6 else (150, 20, 30, 60))
    metal = png(8, 8, lambda x, y: (176, 182, 190, 200) if y % 4 else (140, 146, 154, 200))
    return ramp, seat, metal


def c_bytes(name, data):
    rows = [", ".join("0x%02x" % b for b in data[i:i + 20]) for i in range(0, len(data), 20)]
    return "static const unsigned char %s[%d] = {\n    %s\n};\n" % (name, len(data), ",\n    ".join(rows))


def write_header(path):
    ramp, seat, metal = textures()
    o = ["// Generated by tools/dev/ramps/make_ramps.py: edit the layouts there and run it again.",
         "// Ramps & obstacles (BowlingPlus 1.7.5, Practice). Shared by src/Game.mm and android/native/Game.cpp.",
         "// x: across the lane from its centre (+x is the 2-pin side), y: from the foul line, metres. A ramp is (x, y0, w, h, deg):",
         "// front edge at y0, width w, back edge h above the lane, rising at deg degrees. Every ramp was simulated in pinlab at",
         "// 5.5 to 10 m/s, aimed at its centre and near both edges: the ball climbs it, stays on the lane and is rolling again",
         "// before the pins. Every row of stools leaves a gap a ball fits through.",
         "#pragma once",
         "struct BPRamp { float x, y0, w, h, deg; };",
         "struct BPRampLayout { const char *name; int n; BPRamp r[3]; };",
         "struct BPStool { float x, y; };",
         "struct BPStoolLayout { const char *name; int n; BPStool s[4]; };",
         "static const BPRampLayout kRampLayouts[] = {"]
    for name, rs in RAMP_LAYOUTS:
        o.append('    { "%s", %d, { %s } },' % (name, len(rs), ", ".join("{ %.2ff, %.2ff, %.2ff, %.2ff, %.1ff }" % r for r in rs)))
    o.append("};")
    o.append("static const BPStoolLayout kStoolLayouts[] = {")
    for name, ss in STOOL_LAYOUTS:
        o.append('    { "%s", %d, { %s } },' % (name, len(ss), ", ".join("{ %.2ff, %.2ff }" % s for s in ss)))
    o.append("};")
    o.append("static const int kRampLayoutCount = (int)(sizeof(kRampLayouts) / sizeof(kRampLayouts[0]));")
    o.append("static const int kStoolLayoutCount = (int)(sizeof(kStoolLayouts) / sizeof(kStoolLayouts[0]));")
    o.append("// textures (PNG): hazard stripes for ramps, the seat, the metal")
    o.append(c_bytes("kRampPNG", ramp) + c_bytes("kSeatPNG", seat) + c_bytes("kMetalPNG", metal))
    open(path, "w").write("\n".join(o))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pinlab", default=os.path.join(REPO, "tools/dev/pinlab/pinlab"))
    ap.add_argument("--check-only", action="store_true")
    a = ap.parse_args()
    bad = check_stools()
    rbad, lines = check_ramps(a.pinlab)
    print("\n".join(lines))
    bad += rbad
    print("%d ramp layouts, %d barstool layouts (%d in all)" % (len(RAMP_LAYOUTS), len(STOOL_LAYOUTS), len(RAMP_LAYOUTS) + len(STOOL_LAYOUTS)))
    for n, why in bad: print("FAIL %s: %s" % (n, why))
    if bad: sys.exit(1)
    if not a.check_only:
        write_header(os.path.join(REPO, "src", "RampLayouts.h"))
        print("wrote src/RampLayouts.h")


if __name__ == "__main__":
    main()
