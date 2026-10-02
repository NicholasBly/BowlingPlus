#import <UIKit/UIKit.h>
#import "BFShared.h"
#include <math.h>
#include <stdlib.h>

// The game ships NO full-size skin for Match Up BP (only its small store icon),
// which is why it shows the last ball's skin or plain white. This draws an original
// look-alike swirl in the icon's colours (black / deep navy / gunmetal / silver).
// Layout matches the game's ball sheets: 512x512, two side-by-side halves.

static inline float Hash(int x, int y) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (float)(h & 0xFFFFFF) / 16777215.0f;
}
static inline float Fade(float t) { return t * t * (3.f - 2.f * t); }
static float Noise(float x, float y) {
    int xi = (int)floorf(x), yi = (int)floorf(y);
    float u = Fade(x - xi), v = Fade(y - yi);
    float a = Hash(xi, yi), b = Hash(xi + 1, yi), c = Hash(xi, yi + 1), d = Hash(xi + 1, yi + 1);
    return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
}
static float Fbm(float x, float y) {
    float s = 0.f, amp = 0.5f;
    for (int i = 0; i < 5; i++) {
        s += amp * Noise(x, y);
        x = x * 2.03f + 17.1f;
        y = y * 2.03f + 3.7f;
        amp *= 0.5f;
    }
    return s;
}
static inline float Mix(float a, float b, float t) { return a + (b - a) * t; }
static inline float Step(float e0, float e1, float x) {
    float t = fminf(fmaxf((x - e0) / (e1 - e0), 0.f), 1.f);
    return t * t * (3.f - 2.f * t);
}

NSData *BFMakeMatchUpBPTexturePNG(int size) {
    const int W = size, H = size, half = size / 2;
    uint8_t *px = (uint8_t *)calloc((size_t)W * H * 4, 1);
    if (!px) return nil;
    const float base[3] = {6, 7, 11}, navy[3] = {16, 30, 74}, metal[3] = {58, 66, 82}, silver[3] = {152, 164, 186};
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            float u = (float)(x % half) / (float)half * 3.0f, v = (float)y / (float)H * 6.0f;
            float qx = Fbm(u, v), qy = Fbm(u + 5.2f, v + 1.3f);
            float n = Fbm(u + 2.2f * qx, v + 2.2f * qy);
            float band = 0.5f + 0.5f * sinf(n * 16.0f + qx * 3.0f);
            uint8_t *p = px + ((size_t)y * W + x) * 4;
            for (int i = 0; i < 3; i++) {
                float c = Mix(base[i], navy[i], Step(0.45f, 0.85f, band));
                c = Mix(c, metal[i], Step(0.80f, 0.95f, band) * 0.8f);
                c = Mix(c, silver[i], Step(0.965f, 0.995f, band) * 0.6f);
                p[i] = (uint8_t)fminf(255.f, fmaxf(0.f, c));
            }
            p[3] = 255;
        }
    }
    NSData *png = nil;
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(px, W, H, 8, (size_t)W * 4, cs,
                                             kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    if (ctx) {
        CGImageRef img = CGBitmapContextCreateImage(ctx);
        if (img) {
            png = UIImagePNGRepresentation([UIImage imageWithCGImage:img]);
            CGImageRelease(img);
        }
        CGContextRelease(ctx);
    }
    CGColorSpaceRelease(cs);
    free(px);
    return png;
}
