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
    ("Slalom",            [(-0.21, 5.5), (0.25, 10.5)]),
    ("The gate",          [(-0.33, 10.0), (0.33, 10.0)]),
    ("Centre pair",       [(0.00, 5.0), (0.00, 10.0)]),
    ("Right pair",        [(-0.36, 5.5), (-0.36, 11.0)]),
    ("Left pair",         [(0.36, 5.5), (0.36, 11.0)]),
    ("Diagonal",          [(-0.32, 5.0), (-0.12, 10.5)]),
    ("Cross-over",        [(0.30, 6.0), (-0.30, 12.0)]),
    ("Back gate",         [(-0.34, 13.0), (0.34, 13.0)]),
    ("Offset pair",       [(-0.28, 7.0), (0.22, 12.5)]),
    ("Late stool",        [(0.06, 13.0)]),
]

LANE_HALF = 0.527      # the lane surface's half width (RoadPhysMaterial box)
STOOL_R = 0.15         # stool base radius
BALL_R = 0.108
MIN_GAP = 0.27         # a ball (0.216 m) plus room


LINE_MARGIN = 0.05     # a straight roll must miss every stool by this much (beyond touching)
MAX_STOOLS = 2         # more made the head pin unreachable (1.7.5's fifth build: four stools in a diagonal)
STOOL_MAX_Y = 13.5     # no barstool closer to the head pin (18.26 m) than 4.76 m
GATE_MAX_Y = 14.0      # ...and no gate closer than 4.26 m
HEAD_Y = 18.263
HEAD_REACH = 0.10      # a straight roll must be able to end within 10 cm of the head pin's middle (the pocket or the nose)
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


def head_paths():
    # Roughly how a ball gets to the head pin: straight from the foul line (centre within PLAY_HALF) to a breakpoint
    # (6 to 15 m), then straight on to within HEAD_REACH of the head pin's middle. Each leg at most 6 degrees off the
    # lane's line, and the turn at the breakpoint at most 6 degrees (a hook, not a corner). Yields x(y) as
    # (x0, yb, xb, x1).
    grid = [i * 0.02 - PLAY_HALF for i in range(int(2 * PLAY_HALF / 0.02) + 1)]
    for x0 in grid:
        for yb in [6.0 + 0.5 * k for k in range(19)]:
            for xb in grid:
                s1 = (xb - x0) / yb
                if abs(s1) > 0.105: continue
                for x1 in (-HEAD_REACH, -HEAD_REACH / 2, 0.0, HEAD_REACH / 2, HEAD_REACH):
                    s2 = (x1 - xb) / (HEAD_Y - yb)
                    if abs(s2) > 0.105 or abs(s2 - s1) > 0.105: continue
                    yield x0, yb, xb, x1


def path_x(path, y):
    x0, yb, xb, x1 = path
    return x0 + (xb - x0) * y / yb if y <= yb else xb + (x1 - xb) * (y - yb) / (HEAD_Y - yb)


def stool_head_clearance(st):
    # the best way to the head pin (head_paths): how far it misses the nearest stool, beyond touching it
    best, arg = -9.0, None
    for pth in head_paths():
        c = min(abs(path_x(pth, sy) - sx) for sx, sy in st) - (STOOL_R + BALL_R)
        if c > best: best, arg = c, pth
    return best, arg


def check_stools():
    bad = []
    for name, st in STOOL_LAYOUTS:
        if len(st) > MAX_STOOLS: bad.append((name, "more than %d stools" % MAX_STOOLS))
        for (x, y) in st:
            if not (1.5 <= y <= STOOL_MAX_Y): bad.append((name, "stool at y %.1f outside 1.5..%.1f" % (y, STOOL_MAX_Y)))
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
        h, hp = stool_head_clearance(st)
        print("%-18s ...and the best way to the head pin misses by %.3f m (x %.2f at the foul line, %.2f at %.1f m)" % ("", h, hp[0], hp[2], hp[1]))
        if h < LINE_MARGIN: bad.append((name, "no way to the head pin gets past the stools (best misses by %.3f m)" % h))
    return bad


# Gates (barriers across part of the lane, as in Wii bowling): one or two on one line across the lane, at y, each from
# x0 to x1 (lane-relative, +x the 2-pin side). Two gates leave at least GATE_GAP between them; every layout leaves an
# opening at least GATE_GAP wide that a ball's centre can roll through within PLAY_HALF of the lane's centre.
GATE_LAYOUTS = [
    ("Left barrier",      9.0,  [(0.05, 0.527)]),
    ("Right barrier",     9.0,  [(-0.527, -0.05)]),
    ("Early left",        4.0,  [(-0.05, 0.527)]),
    ("Early right",       4.0,  [(-0.527, 0.05)]),
    ("Centre block",      8.0,  [(-0.12, 0.12)]),
    ("Off-centre block",  11.0, [(0.00, 0.30)]),
    ("Keyhole",           10.0, [(-0.527, -0.22), (0.22, 0.527)]),
    ("Keyhole left",      7.0,  [(-0.527, -0.06), (0.36, 0.527)]),
    ("Keyhole right",     7.0,  [(-0.527, -0.36), (0.06, 0.527)]),
    ("Wide keyhole",      6.0,  [(-0.527, -0.26), (0.26, 0.527)]),
    ("Late keyhole",      13.5, [(-0.527, -0.28), (0.16, 0.527)]),
    ("Pocket door",       14.0, [(-0.527, -0.30), (0.12, 0.527)]),
    ("Two blocks",        8.0,  [(-0.40, -0.20), (0.20, 0.40)]),
    ("Long keyhole",      12.0, [(-0.527, -0.25), (0.17, 0.527)]),
]
GATE_GAP = 0.40        # a ball (0.216 m) with 9 cm to spare on each side


def check_gates():
    bad = []
    for name, y, gs in GATE_LAYOUTS:
        if not (1 <= len(gs) <= 2): bad.append((name, "1 or 2 gates"))
        if not (3.0 <= y <= GATE_MAX_Y): bad.append((name, "gate line at y %.1f outside 3..%.1f" % (y, GATE_MAX_Y)))
        for x0, x1 in gs:
            if x0 >= x1 or x0 < -LANE_HALF - 1e-6 or x1 > LANE_HALF + 1e-6: bad.append((name, "gate off the lane"))
            if x1 - x0 < 0.15: bad.append((name, "gate narrower than 0.15 m"))
        gs = sorted(gs)
        if len(gs) == 2 and gs[1][0] - gs[0][1] < GATE_GAP - 1e-6:
            bad.append((name, "gap between the gates %.2f m" % (gs[1][0] - gs[0][1])))
        # openings: lane edge to the first gate, between gates, last gate to the lane edge
        edges = [-LANE_HALF] + [e for g in gs for e in g] + [LANE_HALF]
        best = 0.0
        for a, b in zip(edges[0::2], edges[1::2]):
            if b - a < GATE_GAP - 1e-6: continue
            lo, hi = max(a + BALL_R + 0.03, -PLAY_HALF), min(b - BALL_R - 0.03, PLAY_HALF)
            best = max(best, hi - lo)
        print("%-18s y %.1f, %d gate%s, room for the ball's centre in the widest opening: %.3f m" % (name, y, len(gs), "" if len(gs) == 1 else "s", best))
        if best < 0.05: bad.append((name, "no opening a ball rolls through comfortably"))
        # some way to the head pin (head_paths) goes through an opening, 3 cm clear of both sides
        ok = any(any(a + BALL_R + 0.03 <= path_x(pth, y) <= b - BALL_R - 0.03 for a, b in zip(edges[0::2], edges[1::2]))
                 for pth in head_paths())
        if not ok: bad.append((name, "no way to the head pin gets through"))
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


def pin_texture(side=512):
    # the barstool's pin: the pin library's Brunswick Max Crown wrap, turned into the game's pin layout by the app's own
    # converter (src/PinWrap.h with the game's template from src/PinGuide.h), as the pin library does
    import io, re, tempfile
    from PIL import Image
    tmp = tempfile.mkdtemp()
    exe = os.path.join(tmp, 'pinlayout')
    subprocess.run(['g++', '-O2', '-std=c++17', '-w', '-I' + os.path.join(REPO, 'src'), os.path.join(HERE, 'pinlayout.cpp'), '-o', exe], check=True)
    h = open(os.path.join(REPO, 'src/PinGuide.h')).read()
    i = h.index('kBPPinTemplatePNG[] = {'); j = h.index('};', i)
    t0 = Image.open(io.BytesIO(bytes(int(x, 16) for x in re.findall(r'0x([0-9a-fA-F]{2})', h[i:j])))).convert('RGBA')
    t = Image.new('RGBA', (side, side), (255, 255, 255, 255)); t.alpha_composite(t0.resize((side, side)))
    open(os.path.join(tmp, 't.rgba'), 'wb').write(t.tobytes())
    ww, wh = 2048, 1024
    w = Image.new('RGBA', (ww, wh), (255, 255, 255, 255))
    w.alpha_composite(Image.open(os.path.join(REPO, 'pins/BrunswickCrownMax.png')).convert('RGBA').resize((ww, wh), Image.LANCZOS))
    open(os.path.join(tmp, 'w.rgba'), 'wb').write(w.tobytes())
    subprocess.run([exe, os.path.join(tmp, 'w.rgba'), str(ww), str(wh), os.path.join(tmp, 't.rgba'), str(side), os.path.join(tmp, 'o.rgba')], check=True)
    out = Image.frombytes('RGBA', (side, side), open(os.path.join(tmp, 'o.rgba'), 'rb').read())
    b = io.BytesIO(); out.save(b, 'PNG', optimize=True)
    return b.getvalue()


def seat_top(side=512):
    # the seat's top, as on Brunswick's pin stools: grey vinyl, the Brunswick B logo (cut from the pin library's Brunswick
    # Max Crown picture, its words turned white and the B royal blue so they show on the vinyl) and three ball holes at
    # the far side. Unity's cylinder maps its top cap u = 0.5 - x, v = 0.5 - z; the seat is turned local y -> world up,
    # so the picture reads upright, unmirrored, for the bowler (top of the picture = down the lane).
    from PIL import Image, ImageDraw, ImageFilter
    src = Image.open(os.path.join(REPO, 'pins/BrunswickCrownMax.png')).convert('RGB')
    sw, sh = src.size
    k = sw / 4096.0
    c = src.crop((int(590 * k), int(975 * k), int(1420 * k), int(1260 * k)))
    c = c.resize((c.size[0] // 2, c.size[1]), Image.LANCZOS)          # the wrap is stretched round the pin: undo it
    w, h = c.size
    navy = Image.new('L', (w, h), 0)
    px, npx = c.load(), navy.load()
    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            npx[x, y] = 255 if (b > r + 40 and r < 160) else 0
    # white that touches the edge is background; white the logo encloses (the crown, the seal) stays white
    outside = navy.copy()
    for x in range(w):
        for y in (0, h - 1):
            if outside.getpixel((x, y)) == 0: ImageDraw.floodfill(outside, (x, y), 128)
    for y in range(h):
        for x in (0, w - 1):
            if outside.getpixel((x, y)) == 0: ImageDraw.floodfill(outside, (x, y), 128)
    split = int(w * 0.255)                                             # left of here: the words; right: the B
    words = navy.filter(ImageFilter.MinFilter(3)).filter(ImageFilter.MaxFilter(3))   # "Brunswick" only, not the fine print
    wp = words.load()
    logo = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    lp, op = logo.load(), outside.load()
    blue, white = (28, 64, 182, 255), (250, 250, 252, 255)            # opaque here: the alpha (the shader's shine) is set below
    for y in range(h):
        for x in range(w):
            if x < split:
                if wp[x, y]: lp[x, y] = white
            elif npx[x, y]: lp[x, y] = blue
            elif op[x, y] == 0: lp[x, y] = white if y < h * 0.5 else blue   # the crown stays white, the seal goes
    top = Image.new('RGBA', (side, side))
    tp = top.load()
    for y in range(side):
        for x in range(side):
            r = ((x - side / 2 + 0.5) ** 2 + (y - side / 2 + 0.5) ** 2) ** 0.5 / (side / 2)
            g = int(150 - 40 * max(0.0, r - 0.72) / 0.28)
            tp[x, y] = (g, g + 2, g + 6, 110)
    lw = int(side * 0.54); lh = int(lw * h / w)
    top.alpha_composite(logo.resize((lw, lh), Image.LANCZOS), ((side - lw) // 2, int(side * 0.47)))
    top.putalpha(110)                                                  # how much the vinyl shines (the logo the same)
    d = ImageDraw.Draw(top)
    for (hx, hy, hr) in ((0.44, 0.17, 0.035), (0.56, 0.17, 0.035), (0.50, 0.31, 0.042)):   # two fingers, a thumb
        cx, cy, rr = hx * side, hy * side, hr * side
        d.ellipse([cx - rr - 3, cy - rr - 3, cx + rr + 3, cy + rr + 3], fill=(175, 178, 184, 160))   # a chrome-ish rim
        d.ellipse([cx - rr, cy - rr, cx + rr, cy + rr], fill=(18, 18, 20, 10))
    return top


def crown(draw, cx, cy, s, fill, sx=1.0):
    # a plain five-point crown: a band with three tall points and two short ones, a dot on each tip
    pts = [(-1.0, 0.55), (-1.0, -0.25), (-0.5, 0.15), (0.0, -0.6), (0.5, 0.15), (1.0, -0.25), (1.0, 0.55)]
    draw.polygon([(cx + x * s * sx, cy + y * s) for x, y in pts], fill=fill)
    for x, y in ((-1.0, -0.25), (0.0, -0.6), (1.0, -0.25)):
        r = 0.17 * s
        draw.ellipse([cx + x * s * sx - r * sx, cy + y * s - r, cx + x * s * sx + r * sx, cy + y * s + r], fill=fill)


def textures():
    import io
    from PIL import Image, ImageDraw
    def enc(img):
        b = io.BytesIO(); img.save(b, 'PNG', optimize=True); return b.getvalue()
    orange, cream = (255, 122, 26, 90), (255, 236, 200, 90)        # alpha: how much of the pin shader's reflection shows
    ramp = png(32, 32, lambda x, y: orange if ((x + y) // 8) % 2 == 0 else cream)
    # the seat's side: royal blue with white crowns all the way round (the cylinder's side wraps the width once). The side
    # is 1.07 m round and 0.08 m tall, 13:1 against the picture's 8:1, so the crowns are drawn 0.6 wide to look right
    k = 4
    band = Image.new('RGBA', (512 * k, 64 * k), (24, 74, 196, 70))
    d = ImageDraw.Draw(band)
    for i in range(10):
        crown(d, (i + 0.5) * 512 * k / 10, 35 * k, 20 * k, (250, 250, 255, 70), 0.6)
    band = band.resize((512, 64), Image.LANCZOS)
    top = seat_top()
    chrome = png(8, 8, lambda x, y: (214, 218, 224, 255) if y % 4 else (170, 176, 184, 255))   # full reflection
    # the gates' panel: red and white diagonal stripes (a barrier), a white rim
    gate = Image.new('RGBA', (256, 64), (250, 250, 250, 60))
    gd = ImageDraw.Draw(gate)
    for i in range(-4, 20):
        x = i * 24
        gd.polygon([(x, 58), (x + 12, 58), (x + 12 + 52, 6), (x + 52, 6)], fill=(214, 28, 36, 60))
    gd.rectangle([0, 0, 255, 5], fill=(250, 250, 250, 60)); gd.rectangle([0, 58, 255, 63], fill=(250, 250, 250, 60))
    return ramp, enc(band), enc(top), chrome, pin_texture(), enc(gate)


def c_bytes(name, data):
    rows = [", ".join("0x%02x" % b for b in data[i:i + 20]) for i in range(0, len(data), 20)]
    return "static const unsigned char %s[%d] = {\n    %s\n};\n" % (name, len(data), ",\n    ".join(rows))


def write_header(path):
    ramp, band, top, chrome, pin, gate = textures()
    o = ["// Generated by tools/dev/ramps/make_ramps.py: edit the layouts there and run it again.",
         "// Ramps & obstacles (BowlingPlus 1.7.5, Practice). Shared by src/Game.mm and android/native/Game.cpp.",
         "// x: across the lane from its centre (+x is the 2-pin side), y: from the foul line, metres. A ramp is (x, y0, w, h, deg):",
         "// front edge at y0, width w, back edge h above the lane, rising at deg degrees. Every ramp was simulated in pinlab at",
         "// 5.5 to 10 m/s, aimed at its centre and near both edges: the ball climbs it, stays on the lane and is rolling again",
         "// before the pins. Every row of stools leaves a gap a ball fits through, and every gate line an opening 0.40 m wide.",
         "#pragma once",
         "struct BPRamp { float x, y0, w, h, deg; };",
         "struct BPRampLayout { const char *name; int n; BPRamp r[3]; };",
         "struct BPStool { float x, y; };",
         "struct BPStoolLayout { const char *name; int n; BPStool s[4]; };",
         "struct BPGate { float x0, x1; };",
         "struct BPGateLayout { const char *name; float y; int n; BPGate g[2]; };",
         "static const BPRampLayout kRampLayouts[] = {"]
    for name, rs in RAMP_LAYOUTS:
        o.append('    { "%s", %d, { %s } },' % (name, len(rs), ", ".join("{ %.2ff, %.2ff, %.2ff, %.2ff, %.1ff }" % r for r in rs)))
    o.append("};")
    o.append("static const BPStoolLayout kStoolLayouts[] = {")
    for name, ss in STOOL_LAYOUTS:
        o.append('    { "%s", %d, { %s } },' % (name, len(ss), ", ".join("{ %.2ff, %.2ff }" % s for s in ss)))
    o.append("};")
    o.append("static const BPGateLayout kGateLayouts[] = {")
    for name, y, gs in GATE_LAYOUTS:
        o.append('    { "%s", %.2ff, %d, { %s } },' % (name, y, len(gs), ", ".join("{ %.3ff, %.3ff }" % g for g in gs)))
    o.append("};")
    o.append("static const int kRampLayoutCount = (int)(sizeof(kRampLayouts) / sizeof(kRampLayouts[0]));")
    o.append("static const int kGateLayoutCount = (int)(sizeof(kGateLayouts) / sizeof(kGateLayouts[0]));")
    o.append("static const int kStoolLayoutCount = (int)(sizeof(kStoolLayouts) / sizeof(kStoolLayouts[0]));")
    o.append("// textures (PNG): hazard stripes for ramps; the barstool's seat side (blue, white crowns) and top (grey), its")
    o.append("// chrome, and its pin (the pin library's Brunswick Max Crown in the game's pin layout, 512 x 512); the gates' panel")
    o.append(c_bytes("kRampPNG", ramp) + c_bytes("kSeatBandPNG", band) + c_bytes("kSeatTopPNG", top) + c_bytes("kChromePNG", chrome) + c_bytes("kStoolPinPNG", pin) + c_bytes("kGatePNG", gate))
    open(path, "w").write("\n".join(o))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pinlab", default=os.path.join(REPO, "tools/dev/pinlab/pinlab"))
    ap.add_argument("--check-only", action="store_true")
    a = ap.parse_args()
    bad = check_stools()
    bad += check_gates()
    rbad, lines = check_ramps(a.pinlab)
    print("\n".join(lines))
    bad += rbad
    print("%d ramp layouts, %d barstool layouts, %d gate layouts (%d in all)" % (len(RAMP_LAYOUTS), len(STOOL_LAYOUTS), len(GATE_LAYOUTS), len(RAMP_LAYOUTS) + len(STOOL_LAYOUTS) + len(GATE_LAYOUTS)))
    for n, why in bad: print("FAIL %s: %s" % (n, why))
    if bad: sys.exit(1)
    if not a.check_only:
        write_header(os.path.join(REPO, "src", "RampLayouts.h"))
        print("wrote src/RampLayouts.h")


if __name__ == "__main__":
    main()
