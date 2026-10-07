// The pin library's spinning 3D preview: a small software renderer (plain C, shared by iOS and Android, and
// testable on its own). It draws the game's own pin mesh (PinMesh.h, the same one the picture layout comes from)
// with a picture in the game's layout, so the preview shows exactly what the pins will look like.
// Conventions (see PIN_TEXTURE_SYSTEM.md): positions are right-handed (exported with X flipped), the pin stands
// along +Y from 0 to 0.381 m, UV v = 1 is the picture's top row. Text that reads correctly in the game reads
// correctly here.
#pragma once
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "PinMesh.h"

typedef struct {
    float n[kBPPinCorners * 3];        // smooth normals, pointing out of the pin
    int ready;
} BPPinShade;

// Smooth normals: each triangle's normal is turned to point away from the pin's axis (the mesh's winding isn't
// relied on), then corners at the same position share the sum of their triangles' normals.
static void PinPreviewNormals(BPPinShade *s) {
    if (s->ready) return;
    memset(s->n, 0, sizeof(s->n));
    float face[3];
    for (int t = 0; t < kBPPinTris; t++) {
        const float *a = kBPPinPos + kBPPinTri[t * 3] * 3, *b = kBPPinPos + kBPPinTri[t * 3 + 1] * 3, *c = kBPPinPos + kBPPinTri[t * 3 + 2] * 3;
        float u[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] }, v[3] = { c[0] - a[0], c[1] - a[1], c[2] - a[2] };
        face[0] = u[1] * v[2] - u[2] * v[1];
        face[1] = u[2] * v[0] - u[0] * v[2];
        face[2] = u[0] * v[1] - u[1] * v[0];
        float cx = (a[0] + b[0] + c[0]) / 3, cy = (a[1] + b[1] + c[1]) / 3, cz = (a[2] + b[2] + c[2]) / 3;
        float ay = cy < 0.02f ? 0.02f : cy > 0.36f ? 0.36f : cy;   // a point inside the pin, on its axis
        float d[3] = { cx, cy - ay, cz };
        if (cy < 0.002f) { d[0] = 0; d[1] = -1; d[2] = 0; }        // the flat underside
        if (face[0] * d[0] + face[1] * d[1] + face[2] * d[2] < 0) { face[0] = -face[0]; face[1] = -face[1]; face[2] = -face[2]; }
        for (int k = 0; k < 3; k++) {
            float *n = s->n + kBPPinTri[t * 3 + k] * 3;
            n[0] += face[0]; n[1] += face[1]; n[2] += face[2];
        }
    }
    // corners split for the picture layout share a position: give them the same (summed) normal
    float *sum = (float *)calloc(kBPPinCorners * 3, sizeof(float));
    if (sum) {
        for (int i = 0; i < kBPPinCorners; i++)
            for (int j = 0; j < kBPPinCorners; j++) {
                const float *p = kBPPinPos + i * 3, *q = kBPPinPos + j * 3;
                if (fabsf(p[0] - q[0]) < 1e-5f && fabsf(p[1] - q[1]) < 1e-5f && fabsf(p[2] - q[2]) < 1e-5f) {
                    sum[i * 3] += s->n[j * 3]; sum[i * 3 + 1] += s->n[j * 3 + 1]; sum[i * 3 + 2] += s->n[j * 3 + 2];
                }
            }
        memcpy(s->n, sum, sizeof(s->n));
        free(sum);
    }
    for (int i = 0; i < kBPPinCorners; i++) {
        float *n = s->n + i * 3, l = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (l > 0) { n[0] /= l; n[1] /= l; n[2] /= l; }
    }
    s->ready = 1;
}

static inline void PinTexel(const uint8_t *tex, int side, float u, float v, float out[3]) {   // bilinear
    float x = u * side - 0.5f, y = (1.0f - v) * side - 0.5f;
    int x0 = (int)floorf(x), y0 = (int)floorf(y);
    float fx = x - x0, fy = y - y0;
    for (int c = 0; c < 3; c++) out[c] = 0;
    for (int k = 0; k < 4; k++) {
        int xi = x0 + (k & 1), yi = y0 + (k >> 1);
        xi = xi < 0 ? 0 : xi >= side ? side - 1 : xi;
        yi = yi < 0 ? 0 : yi >= side ? side - 1 : yi;
        float w = ((k & 1) ? fx : 1 - fx) * ((k >> 1) ? fy : 1 - fy);
        const uint8_t *p = tex + ((size_t)yi * side + xi) * 4;
        out[0] += w * p[0]; out[1] += w * p[1]; out[2] += w * p[2];
    }
}

// Draws the pin turned `angle` radians about its axis into out (w x h RGBA, rows top-down, background
// transparent). tex: the picture in the game's layout, side x side RGBA. zbuf: w * h floats (scratch).
static void PinPreviewRender(BPPinShade *s, const uint8_t *tex, int side, float angle, uint8_t *out, int w, int h, float *zbuf) {
    PinPreviewNormals(s);
    memset(out, 0, (size_t)w * h * 4);
    for (int i = 0; i < w * h; i++) zbuf[i] = 1e30f;
    float ca = cosf(angle), sa = sinf(angle);
    // camera: in front of the pin (+z), level with its middle, slightly above; the pin fills the height
    const float camY = 0.19f, camZ = 0.95f, tilt = 0.05f;      // tilt: looking down a little (radians)
    float ct = cosf(tilt), st = sinf(tilt);
    float f = 0.84f * h / (0.40f / camZ);                       // the pin (0.381 m) fills about 80% of the height
    const float L[3] = { -0.45f, 0.55f, 0.70f };               // light: upper left, in front
    float ll = sqrtf(L[0] * L[0] + L[1] * L[1] + L[2] * L[2]);
    float lx = L[0] / ll, ly = L[1] / ll, lz = L[2] / ll;
    float hx = lx, hy = ly, hz = lz + 1.0f, hl = sqrtf(hx * hx + hy * hy + hz * hz);   // half vector (viewer at +z)
    hx /= hl; hy /= hl; hz /= hl;
    float NN[kBPPinCorners * 3], S[kBPPinCorners * 3];   // per call (renderers on different threads don't share)
    for (int i = 0; i < kBPPinCorners; i++) {
        const float *p = kBPPinPos + i * 3, *n = s->n + i * 3;
        float x = p[0] * ca + p[2] * sa, z = -p[0] * sa + p[2] * ca, y = p[1];   // turn about Y
        float nx = n[0] * ca + n[2] * sa, nz = -n[0] * sa + n[2] * ca, ny = n[1];
        float ry = y - camY, rz = z - camZ;                                     // into camera space
        float y2 = ry * ct + rz * st, z2 = -ry * st + rz * ct;
        NN[i * 3] = nx; NN[i * 3 + 1] = ny * ct + nz * st; NN[i * 3 + 2] = -ny * st + nz * ct;
        float d = -z2 > 0.01f ? -z2 : 0.01f;
        S[i * 3] = w * 0.5f + f * x / d;
        S[i * 3 + 1] = h * 0.5f - f * y2 / d - h * 0.107f;     // (centred: the base is nearer, so it draws bigger)
        S[i * 3 + 2] = d;
    }
    for (int t = 0; t < kBPPinTris; t++) {
        int ia = kBPPinTri[t * 3], ib = kBPPinTri[t * 3 + 1], ic = kBPPinTri[t * 3 + 2];
        float x0 = S[ia * 3], y0 = S[ia * 3 + 1], x1 = S[ib * 3], y1 = S[ib * 3 + 1], x2 = S[ic * 3], y2 = S[ic * 3 + 1];
        float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
        if (fabsf(area) < 1e-6f) continue;
        int minx = (int)floorf(fminf(x0, fminf(x1, x2))), maxx = (int)ceilf(fmaxf(x0, fmaxf(x1, x2)));
        int miny = (int)floorf(fminf(y0, fminf(y1, y2))), maxy = (int)ceilf(fmaxf(y0, fmaxf(y1, y2)));
        minx = minx < 0 ? 0 : minx;
        miny = miny < 0 ? 0 : miny;
        maxx = maxx > w - 1 ? w - 1 : maxx;
        maxy = maxy > h - 1 ? h - 1 : maxy;
        float iz0 = 1 / S[ia * 3 + 2], iz1 = 1 / S[ib * 3 + 2], iz2 = 1 / S[ic * 3 + 2];
        for (int py = miny; py <= maxy; py++)
            for (int px = minx; px <= maxx; px++) {
                float sx = px + 0.5f, sy = py + 0.5f;
                float w0 = ((x1 - sx) * (y2 - sy) - (x2 - sx) * (y1 - sy)) / area;
                float w1 = ((x2 - sx) * (y0 - sy) - (x0 - sx) * (y2 - sy)) / area;
                float w2 = 1 - w0 - w1;
                if (w0 < 0 || w1 < 0 || w2 < 0) continue;
                float iz = w0 * iz0 + w1 * iz1 + w2 * iz2, d = 1 / iz;   // perspective-correct
                size_t o = (size_t)py * w + px;
                if (d >= zbuf[o]) continue;
                zbuf[o] = d;
                float b0 = w0 * iz0 * d, b1 = w1 * iz1 * d, b2 = w2 * iz2 * d;
                float u = b0 * kBPPinUV[ia * 2] + b1 * kBPPinUV[ib * 2] + b2 * kBPPinUV[ic * 2];
                float v = b0 * kBPPinUV[ia * 2 + 1] + b1 * kBPPinUV[ib * 2 + 1] + b2 * kBPPinUV[ic * 2 + 1];
                float nx = b0 * NN[ia * 3] + b1 * NN[ib * 3] + b2 * NN[ic * 3];
                float ny = b0 * NN[ia * 3 + 1] + b1 * NN[ib * 3 + 1] + b2 * NN[ic * 3 + 1];
                float nz = b0 * NN[ia * 3 + 2] + b1 * NN[ib * 3 + 2] + b2 * NN[ic * 3 + 2];
                float nl = sqrtf(nx * nx + ny * ny + nz * nz);
                if (nl > 0) { nx /= nl; ny /= nl; nz /= nl; }
                float c[3];
                PinTexel(tex, side, u, v, c);
                float dif = nx * lx + ny * ly + nz * lz; if (dif < 0) dif = 0;
                float spec = nx * hx + ny * hy + nz * hz; spec = spec > 0 ? powf(spec, 48.0f) : 0;
                float rim = 1.0f - (nz > 0 ? nz : 0); rim = rim * rim * 0.18f;            // a little edge light
                float k = 0.42f + 0.62f * dif;
                uint8_t *q = out + o * 4;
                for (int ch = 0; ch < 3; ch++) {
                    float val = c[ch] * k + 255.0f * (0.45f * spec + rim);
                    q[ch] = (uint8_t)(val > 255 ? 255 : val < 0 ? 0 : val);
                }
                q[3] = 255;
            }
    }
}
