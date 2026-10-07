// Host harness for the pin library preview: converts a wrap sheet with the app's own code (PinWrap.h) and
// renders it with the app's preview renderer (PinPreview.h). Driven by run.py.
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "PinWrap.h"
#include "PinPreview.h"
// usage: pv wrap.rgba ww wh tmpl.rgba side out_prefix nframes W H
int main(int argc, char **argv) {
    int ww = atoi(argv[2]), wh = atoi(argv[3]), side = atoi(argv[5]), nf = atoi(argv[7]), W = atoi(argv[8]), H = atoi(argv[9]);
    std::vector<uint8_t> wrap((size_t)ww * wh * 4), tmpl((size_t)side * side * 4), lay((size_t)side * side * 4);
    FILE *f = fopen(argv[1], "rb"); if (!f || fread(wrap.data(), 1, wrap.size(), f) != wrap.size()) return 1; fclose(f);
    f = fopen(argv[4], "rb"); if (!f || fread(tmpl.data(), 1, tmpl.size(), f) != tmpl.size()) return 1; fclose(f);
    PinWrapToLayout(wrap.data(), ww, wh, tmpl.data(), lay.data(), side);
    PinFillAround(lay.data(), side, 0);
    static BPPinShade sh;
    std::vector<uint8_t> out((size_t)W * H * 4);
    std::vector<float> z((size_t)W * H);
    char name[512];
    for (int k = 0; k < nf; k++) {
        PinPreviewRender(&sh, lay.data(), side, 6.2831853f * k / nf, out.data(), W, H, z.data());
        snprintf(name, sizeof(name), "%s%02d.rgba", argv[6], k);
        f = fopen(name, "wb"); fwrite(out.data(), 1, out.size(), f); fclose(f);
    }
    return 0;
}
