#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include "../../src/KegelParse.h"
#include "shipped_all.inc"
static float V[41][240], G[41][60];
int main(int argc, char **argv) {
    for (int a = 1; a < argc; a++) {
        std::ifstream in(argv[a]); std::stringstream ss; ss << in.rdbuf(); std::string all = ss.str();
        std::vector<std::string> L; size_t p = 0;
        while (true) { size_t q = all.find('\n', p); if (q == std::string::npos) { L.push_back(all.substr(p)); break; } L.push_back(all.substr(p, q - p)); p = q + 1; }
        KegelFile f = KegelParseLines(L);
        std::vector<KStep> F, R;
        for (auto &k : f.fwd) F.push_back({k.start, k.stop, k.loads, k.speed, k.end, (float)f.ul});
        for (auto &k : f.rev) R.push_back({k.start, k.stop, k.loads, k.speed, k.end, (float)f.ul});
        std::vector<KStep> fo, ro;
        KegelExactChainV(F, R, f.drop, true, fo, ro);
        KegelDrawExactK(fo, ro, f.drop, f.feet, V);
        std::vector<KStep> F2, R2; KegelEnds_dummy:;
        // the game's own model on the same file data (rounded ends, as the game's engine reads them)
        KegelDraw(F, R, f.drop > 0 ? f.drop : 60, G, false);
        double sv = 0, sg = 0, mx = 0; int bad = 0, nz = 0;
        for (int b = 1; b < 40; b++) for (int r = 0; r < 240; r++) { float v = V[b][r]; if (!(v == v) || v < 0 || v > 400) bad++; sv += v; if (v > mx) mx = v; if (v > 0.01) nz++; }
        for (int b = 1; b < 40; b++) for (int r = 0; r < 60; r++) sg += G[b][r];
        // film everywhere? boards 2..38 inside the distance must all be > 0
        int holes = 0; for (int b = 2; b <= 38; b++) for (int r = 0; r < (int)(f.feet * 4) - 1; r++) if (V[b][r] < 0.5f) holes++;
        // outside-the-distance must be empty
        double beyond = 0; for (int b = 1; b < 40; b++) for (int r = (int)(f.feet * 4) + 1; r < 240; r++) beyond += V[b][r];
        printf("%s|%d|%d|%.1f|%.2f|%d|%d|%.2f\n", argv[a], f.feet, f.drop, mx, (sv / 4) / (sg > 0 ? sg : 1), bad, holes, beyond);
        FILE *o = fopen((std::string(argv[a]) + ".bin").c_str(), "wb"); fwrite(V, sizeof V, 1, o); fclose(o);
    }
}
