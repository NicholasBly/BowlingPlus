# Checking the oil model against Kegel's own charts

`verify.py` compares, for every board and every quarter foot, whether Kegel's chart (PDF) shows an oil pass and whether
BowlingPlus's model does. 2025 U.S. Open #4, 2017 SEA Games Long, 2026 Regional 37 and the Start 5 / Stop 15 calibration
pattern agree on 99.90% of 23,088 cells; the rest are the black arrow triangles drawn on the chart.

**The game's own 48 patterns:** `extract_game_patterns.py` (needs UnityPy and the decrypted IPA) writes `files/NN.txt`;
`all48.cpp` (build it like `drv.cpp`, after `extract.py`) runs the model on each and prints distance, drop, max value,
total oil versus the game's engine, film holes, and oil past the distance. All 48 come out clean.

1. `python3 tools/oil_verify/extract.py` (from the repo root) writes `shipped.inc`: the exact code in `src/Game.mm`.
2. `cd tools/oil_verify && g++ -std=c++17 -O1 -o drv drv.cpp`
3. Put the Kegel PDFs here as `us4.pdf`, `sea.pdf`, `r37.pdf`, `cal.pdf`, then run `pdftotext -raw X.pdf X.txt` and
   `pdftoppm -r 400 -png -f 1 -l 1 X.pdf chart_X` for each. (Needs poppler-utils.)
4. `python3 verify.py`

The chart geometry the script relies on (400 dpi crop, fractions 0.655-0.935 x 0.255-0.88 of the page): board columns
start at x = 59.5 px with a 21.86 px pitch, the 55 ft line is at y = 245.5 px with 45.2 px per foot. See VERIFIED_NOTES.md,
"Kegel's chart, measured".
