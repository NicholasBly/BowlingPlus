#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include "shipped.inc"
static float V[41][240];
int main(int argc, char **argv) {
    // input: line 1: drop feet; then "F|R start stop loads speed ul end"
    FILE *f = fopen(argv[1], "r"); int drop, feet; if (fscanf(f, "%d %d", &drop, &feet) != 2) return 1;
    std::vector<KStep> F, R; char d; int a, b, l, s; float ul, e;
    while (fscanf(f, " %c %d %d %d %d %f %f", &d, &a, &b, &l, &s, &ul, &e) == 7) (d == 'F' ? F : R).push_back({a, b, l, s, e, ul});
    fclose(f);
    std::vector<KStep> fo, ro; KegelExactChainV(F, R, drop, false, fo, ro);
    KegelDrawExactK(fo, ro, drop, feet, V);
    FILE *o = fopen(argv[2], "wb"); fwrite(V, sizeof V, 1, o); fclose(o);
}
