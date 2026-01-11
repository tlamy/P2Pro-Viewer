#include "ColorConversion.hpp"
#include <algorithm>

namespace ColorConversion {

static inline uint8_t clamp(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

void YUY2toRGB(const uint8_t* yuy2, uint8_t* rgb, int width, int height) {
    int total_pixels = width * height;
    for (int i = 0, j = 0; i < total_pixels * 2; i += 4, j += 6) {
        int y0 = yuy2[i];
        int u  = yuy2[i + 1] - 128;
        int y1 = yuy2[i + 2];
        int v  = yuy2[i + 3] - 128;

        // Using BT.601 full range coefficients
        int r_off = (359 * v) >> 8;
        int g_off = (88 * u + 183 * v) >> 8;
        int b_off = (454 * u) >> 8;

        rgb[j]     = clamp(y0 + r_off);
        rgb[j + 1] = clamp(y0 - g_off);
        rgb[j + 2] = clamp(y0 + b_off);

        rgb[j + 3] = clamp(y1 + r_off);
        rgb[j + 4] = clamp(y1 - g_off);
        rgb[j + 5] = clamp(y1 + b_off);
    }
}

void RGBtoBGR(const uint8_t* rgb, uint8_t* bgr, int width, int height) {
    int total_bytes = width * height * 3;
    for (int i = 0; i < total_bytes; i += 3) {
        bgr[i]     = rgb[i + 2];
        bgr[i + 1] = rgb[i + 1];
        bgr[i + 2] = rgb[i];
    }
}

}
