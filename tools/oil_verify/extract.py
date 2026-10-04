#!/usr/bin/env python3
"""Extract the shipped Kegel-accurate oil code from src/Game.mm into shipped.inc (plain C++), so the
test driver compiles exactly what the tweak runs. Run from the repository root:
    python3 tools/oil_verify/extract.py
"""
src = open("src/Game.mm").read()
a = src.index("struct KStep {"); b = src.index("// Step ends: exact")
c = src.index("static const float kFilmFront"); d = src.index("static std::vector<KStep> KegelParseSteps(")
e = src.index("static void KegelDrawExactK("); f = src.index("static NSDictionary *KegelDrawPattern(")
open("tools/oil_verify/shipped.inc", "w").write(src[a:b] + src[c:d] + src[e:f])
g0 = src.index("// fillGaps (BowlingPlus improvement"); g1 = src.index("// custom = a BowlingPlus pattern")
open("tools/oil_verify/shipped_all.inc", "w").write(src[a:b] + src[g0:g1] + src[c:d] + src[e:f])   # + the game's own model, for comparison
print("wrote tools/oil_verify/shipped.inc and shipped_all.inc")
