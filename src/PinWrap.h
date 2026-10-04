// Pin picture helpers (plain C, so they can be tested on their own).
#pragma once
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "PinMask.h"
#include "PinMesh.h"

static inline int PinShapeAt(int x, int y, int side) {
    int i = (y * kBPPinMaskSize / side) * kBPPinMaskSize + (x * kBPPinMaskSize / side);
    return (kBPPinMask[i >> 3] >> (7 - (i & 7))) & 1;
}

// "Wrap" layout -> the game's pin picture. The wrap is the pin's surface unrolled evenly: left to right
// once around the pin (the left and right edges meet), top to bottom from the top of the head (0.381 m)
// to the base. Side A (the left half-pin in the game's layout, angle 270) is at 1/4 of the width, side B
// (angle 90) at 3/4; the game's two seams are at the edges and the middle. Every pixel of the game's
// picture that lands on the pin is traced to its 3D point on the pin mesh and colored from the wrap,
// so both halves agree at the seams. The flat underside keeps the template's (the game's own) look.
// wrap: ww x wh RGBA rows top-down; tmpl: side x side RGBA or NULL; out: side x side RGBA.
static void PinWrapToLayout(const uint8_t *wrap, int ww, int wh, const uint8_t *tmpl, uint8_t *out, int side) {
    for (int t = 0; t < kBPPinTris; t++) {
        int c[3] = { kBPPinTri[t * 3], kBPPinTri[t * 3 + 1], kBPPinTri[t * 3 + 2] };
        float X[3], Y[3], P[3][3];
        int under = 1;
        for (int k = 0; k < 3; k++) {
            X[k] = kBPPinUV[c[k] * 2] * side;
            Y[k] = (1.f - kBPPinUV[c[k] * 2 + 1]) * side;
            for (int j = 0; j < 3; j++) P[k][j] = kBPPinPos[c[k] * 3 + j];
            if (P[k][1] > 0.002f) under = 0;
        }
        float den = (Y[1] - Y[2]) * (X[0] - X[2]) + (X[2] - X[1]) * (Y[0] - Y[2]);
        if (fabsf(den) < 1e-9f) continue;
        int x0 = (int)fmaxf(0, floorf(fminf(X[0], fminf(X[1], X[2])))), x1 = (int)fminf(side - 1, ceilf(fmaxf(X[0], fmaxf(X[1], X[2]))));
        int y0 = (int)fmaxf(0, floorf(fminf(Y[0], fminf(Y[1], Y[2])))), y1 = (int)fminf(side - 1, ceilf(fmaxf(Y[0], fmaxf(Y[1], Y[2]))));
        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) {
                float px = x + 0.5f, py = y + 0.5f;
                float w0 = ((Y[1] - Y[2]) * (px - X[2]) + (X[2] - X[1]) * (py - Y[2])) / den;
                float w1 = ((Y[2] - Y[0]) * (px - X[2]) + (X[0] - X[2]) * (py - Y[2])) / den;
                float w2 = 1.f - w0 - w1;
                if (w0 < -1e-4f || w1 < -1e-4f || w2 < -1e-4f) continue;
                uint8_t *o = out + ((size_t)y * side + x) * 4;
                if (under) {
                    if (tmpl) memcpy(o, tmpl + ((size_t)y * side + x) * 4, 4);
                    else o[0] = o[1] = o[2] = o[3] = 255;
                    continue;
                }
                float qx = w0 * P[0][0] + w1 * P[1][0] + w2 * P[2][0];
                float qy = w0 * P[0][1] + w1 * P[1][1] + w2 * P[2][1];
                float qz = w0 * P[0][2] + w1 * P[1][2] + w2 * P[2][2];
                float ang = atan2f(qz, qx) * 57.29578f;                     // degrees
                float u = fmodf(720.f - ang, 360.f) / 360.f;                // 0..1 around the pin (reads right, not mirrored)
                float v = 1.f - fminf(fmaxf(qy / 0.381f, 0.f), 1.f);        // 0 = top of the head
                float fx = u * ww - 0.5f, fy = v * wh - 0.5f;               // bilinear, wrapping around
                int ix = (int)floorf(fx), iy = (int)floorf(fy);
                float ax = fx - ix, ay = fy - iy;
                for (int ch = 0; ch < 4; ch++) {
                    float s = 0;
                    for (int dy = 0; dy < 2; dy++) for (int dx = 0; dx < 2; dx++) {
                        int sx = ((ix + dx) % ww + ww) % ww, sy = iy + dy;
                        sy = sy < 0 ? 0 : sy >= wh ? wh - 1 : sy;
                        s += wrap[((size_t)sy * ww + sx) * 4 + ch] * (dx ? ax : 1 - ax) * (dy ? ay : 1 - ay);
                    }
                    o[ch] = (uint8_t)fminf(255.f, s + 0.5f);
                }
            }
        }
    }
}

// Seam fix. The GPU blends in pixels from just outside a half-pin shape at its edges (more on small,
// far pins: mipmaps), so whatever is around the shapes shows as a crack down the pin's side. Every pixel
// outside the shapes (and their outermost 2 px, which also covers the rougher low-quality, reflection
// and pinsetter models) takes the color of the nearest pixel inside, and all is made solid like the
// game's own pin picture (the shader uses alpha as its shininess mask). cleanEdges: a hand-drawn
// layout's outline can sit a little inside the real shapes, leaving slivers of its background on the
// pin: pixels near the shape edges that match the background color are refilled from inside, and so is
// a thin band right at the edges (its pixels are often a blend of the background and the design).
static void PinFillAround(uint8_t *px, int side, int cleanEdges) {
    int n = side * side, head = 0, tail = 0;
    int *queue = (int *)malloc(sizeof(int) * n);
    uint8_t *keep = (uint8_t *)malloc(n), *done = (uint8_t *)calloc(n, 1);
    if (!queue || !keep || !done) { free(queue); free(keep); free(done); return; }
    for (int y = 0; y < side; y++) for (int x = 0; x < side; x++) keep[y * side + x] = (uint8_t)PinShapeAt(x, y, side);
    if (cleanEdges) {
        static int hist[32768];                     // the background: the most common color outside the shapes
        memset(hist, 0, sizeof(hist));
        for (int i = 0; i < n; i++) if (!keep[i]) { uint8_t *p = px + i * 4; hist[(p[0] >> 3) << 10 | (p[1] >> 3) << 5 | (p[2] >> 3)]++; }
        int best = 0;
        for (int i = 1; i < 32768; i++) if (hist[i] > hist[best]) best = i;
        int br = ((best >> 10) & 31) * 8 + 4, bg = ((best >> 5) & 31) * 8 + 4, bb = (best & 31) * 8 + 4;
        int reach = side / 64;                       // ~16 px at 1024: how far inside an edge to look
        int band = side / 170;                       // ~6 px at 1024: always refilled (blended edge pixels)
        uint8_t *dist = (uint8_t *)malloc(n);
        if (dist) {
            memset(dist, 255, n);
            for (int i = 0; i < n; i++) if (!keep[i]) { dist[i] = 0; queue[tail++] = i; }
            while (head < tail) {
                int i = queue[head++], x = i % side, y = i / side;
                if (dist[i] >= reach) continue;
                int nb[4] = { x > 0 ? i - 1 : -1, x < side - 1 ? i + 1 : -1, y > 0 ? i - side : -1, y < side - 1 ? i + side : -1 };
                for (int k = 0; k < 4; k++) {
                    int j = nb[k];
                    if (j < 0 || dist[j] != 255) continue;
                    dist[j] = dist[i] + 1;
                    queue[tail++] = j;
                    uint8_t *p = px + j * 4;
                    if (dist[j] <= band || abs(p[0] - br) + abs(p[1] - bg) + abs(p[2] - bb) < 40) keep[j] = 0;
                }
            }
            free(dist);
        }
        head = tail = 0;
    }
    for (int i = 0; i < n; i++) if (keep[i]) { done[i] = 1; queue[tail++] = i; }
    while (head < tail) {                           // nearest-first flood out from the shapes
        int i = queue[head++], x = i % side, y = i / side;
        int nb[4] = { x > 0 ? i - 1 : -1, x < side - 1 ? i + 1 : -1, y > 0 ? i - side : -1, y < side - 1 ? i + side : -1 };
        for (int k = 0; k < 4; k++) {
            int j = nb[k];
            if (j < 0 || done[j]) continue;
            done[j] = 1;
            memcpy(px + j * 4, px + i * 4, 3);
            queue[tail++] = j;
        }
    }
    for (int i = 0; i < n; i++) px[i * 4 + 3] = 255;
    free(queue); free(keep); free(done);
}
