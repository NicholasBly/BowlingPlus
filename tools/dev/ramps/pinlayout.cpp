// Turns a pin wrap picture (2:1) into the game's pin texture layout with the app's own code (src/PinWrap.h), for the
// barstools' pin (make_ramps.py). usage: pinlayout wrap.rgba ww wh tmpl.rgba side out.rgba
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "PinWrap.h"
int main(int argc, char **argv) {
    if (argc != 7) return 2;
    int ww = atoi(argv[2]), wh = atoi(argv[3]), side = atoi(argv[5]);
    std::vector<uint8_t> wrap((size_t)ww * wh * 4), tmpl((size_t)side * side * 4), lay((size_t)side * side * 4);
    FILE *f = fopen(argv[1], "rb"); if (!f || fread(wrap.data(), 1, wrap.size(), f) != wrap.size()) return 1; fclose(f);
    f = fopen(argv[4], "rb"); if (!f || fread(tmpl.data(), 1, tmpl.size(), f) != tmpl.size()) return 1; fclose(f);
    PinWrapToLayout(wrap.data(), ww, wh, tmpl.data(), lay.data(), side);
    PinFillAround(lay.data(), side, 0);
    f = fopen(argv[6], "wb"); if (!f) return 1; fwrite(lay.data(), 1, lay.size(), f); fclose(f);
    return 0;
}
