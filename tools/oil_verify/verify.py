import re, subprocess, struct, sys
import numpy as np
from PIL import Image
ROW = re.compile(r'(?<![\d.,])(\d{1,2})\s+(\d{1,2}\s?[LR]|20)\s+(\d{1,2}\s?[LR]|20)\s+(\d{1,2})\s+(\d{1,3})\s+(\d{1,2})\s+(\d{1,4})'
                 r'\s+(?:[A-Za-z][^\d]{0,24}?)?\s*(\d{1,2}(?:\.\d+)?)\s*(?:\u2192|->|>|\u2013|-|to)?\s*(\d{1,2}(?:\.\d+)?)\s+[\d,]+', re.I)
FEET = re.compile(r'(\d{1,2})\s*FEET', re.I)
def board(t):
    t = t.replace(' ', '').upper(); n = int(re.match(r'\d+', t).group()); return 40 - n if t.endswith('R') else n
def steps_from(text):
    m0 = re.search(r'REVERSE\s+LOADS', text, re.I); rev_at = m0.start() if m0 else -1
    fr, last = [[], []], [0, 0]
    for m in ROW.finditer(text):
        num = int(m.group(1)); d = 1 if m.start() > rev_at else 0
        if num != last[d] + 1: continue
        last[d] = num
        a, b = board(m.group(2)), board(m.group(3))
        fr[d].append((min(a, b), max(a, b), int(m.group(4)), int(m.group(6)), int(m.group(5)), float(m.group(9))))
    ft = FEET.findall(text); drop = int(ft[1]) if len(ft) > 1 else 0
    m1 = re.search(r'DROP\s*BRUSH:', text, re.I)
    if m1:
        mm = FEET.search(text[m1.end(): m1.end() + 24]); drop = int(mm.group(1)) if mm else drop
    return int(ft[0]), drop, fr
X0, PITCH, Y55, PPF = 59.5, 21.86, 245.5, 45.2
def chart_dark(path):
    im = Image.open(path).convert('RGB'); W, H = im.size
    c = np.asarray(im.crop((int(W*0.655), int(H*0.255), int(W*0.935), int(H*0.88)))).astype(int)
    def dark(b, ft):
        y = int(Y55 + (55 - ft) * PPF) + 4          # +4 px = the 0.1 ft calibration offset measured at the foul line
        x = int(X0 + (b - 0.5) * PITCH) - 3
        return 255 - int(np.median(c[y - 2:y + 3, x - 1:x + 1, 1]))
    return dark
tot_all = ok_all = 0
for name in ('us4', 'sea', 'r37', 'cal'):
    text = open(f'./{name}.txt').read()
    dist, drop, fr = steps_from(text)
    with open(f'./{name}.in', 'w') as f:
        f.write(f'{drop} {dist}\n')
        for d, L in (('F', fr[0]), ('R', fr[1])):
            for a, b, l, s, ul, e in L: f.write(f'{d} {a} {b} {l} {s} {ul} {e}\n')
    subprocess.run(['./drv', f'./{name}.in', f'./{name}.bin'], check=True)
    raw = open(f'./{name}.bin', 'rb').read()
    V = np.array([struct.unpack_from('<240f', raw, b * 960) for b in range(41)])
    dark = chart_dark(f'./chart_{name}-1.png')
    tot = ok = skipped = 0; bad = []
    for r in range(1, int(dist * 4) - 1):                 # every quarter foot inside the oiled distance
        ft = (r + 0.5) / 4
        cd = np.array([dark(b, ft) for b in range(2, 39)])
        mv = V[2:39, r]
        base_c, base_m = cd.min(), mv.min()
        if (cd > base_c + 20).all():                       # every board has a pass: no film-only baseline here
            skipped += 1; continue
        pc = cd > base_c + 20; pm = mv > base_m + 6
        # tolerate an edge landing in the neighbouring row (the chart is continuous, the map has 0.25 ft rows)
        for i in range(37):
            agree = pc[i] == pm[i]
            if not agree:
                for rr in (r - 1, r + 1):
                    if rr < 0 or rr >= 240: continue
                    cd2 = np.array([dark(2 + i, (rr + 0.5) / 4)]); mv2 = V[2 + i, rr]
                    b2m = V[2:39, rr].min()
                    if (cd2[0] > base_c + 20) == (mv2 > b2m + 6) or (cd2[0] > base_c + 20) == pm[i]:
                        agree = True; break
            tot += 1; ok += agree
            if not agree: bad.append((round(ft, 2), 2 + i))
    tot_all += tot; ok_all += ok
    print(f'{name:5s} dist {dist} drop {drop}: {len(fr[0])} fwd + {len(fr[1])} rev steps | cells compared {tot}, agree {ok} = {100 * ok / tot:.2f}%  (rows skipped {skipped}) | disagreements: {bad[:6]}')
print(f'ALL: {ok_all} of {tot_all} cells = {100 * ok_all / tot_all:.2f}%')
