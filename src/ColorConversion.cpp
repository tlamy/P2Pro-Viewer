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

void RGBtoBGRFlipped(const uint8_t* rgb, uint8_t* bgr, int width, int height) {
    int stride = width * 3;
    for (int y = 0; y < height; ++y) {
        const uint8_t* src_row = rgb + (y * stride);
        uint8_t* dst_row = bgr + ((height - 1 - y) * stride);
        for (int x = 0; x < width; ++x) {
            dst_row[x * 3]     = src_row[x * 3 + 2];
            dst_row[x * 3 + 1] = src_row[x * 3 + 1];
            dst_row[x * 3 + 2] = src_row[x * 3];
        }
    }
}

void drawTTFText(uint8_t* rgb, int width, int height, int x, int y, const char* text, TTF_Font* font, SDL_Color color, SDL_Color shadowColor) {
    if (!font || !text || !*text) return;

    auto renderToBuffer = [&](const char* t, SDL_Color c, int ox, int oy) {
        SDL_Surface* surface = TTF_RenderText_Blended(font, t, c);
        if (!surface) return;

        SDL_Surface* rgbSurface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGB24, 0);
        if (rgbSurface) {
            for (int sy = 0; sy < rgbSurface->h; ++sy) {
                for (int sx = 0; sx < rgbSurface->w; ++sx) {
                    int dx = x + sx + ox;
                    int dy = y + sy + oy;
                    if (dx >= 0 && dx < width && dy >= 0 && dy < height) {
                        uint8_t* src = (uint8_t*)rgbSurface->pixels + sy * rgbSurface->pitch + sx * 3;
                        uint8_t* dst = rgb + (dy * width + dx) * 3;
                        
                        // Simple alpha blending if we had alpha, but TTF_RenderText_Blended gives alpha.
                        // However, SDL_ConvertSurfaceFormat to RGB24 drops alpha.
                        // Let's use the original surface to get alpha.
                        uint8_t alpha = 255;
                        if (surface->format->BytesPerPixel == 4) {
                            uint8_t* src_alpha = (uint8_t*)surface->pixels + sy * surface->pitch + sx * 4;
                            alpha = src_alpha[3];
                        }

                        if (alpha > 0) {
                            dst[0] = (src[0] * alpha + dst[0] * (255 - alpha)) / 255;
                            dst[1] = (src[1] * alpha + dst[1] * (255 - alpha)) / 255;
                            dst[2] = (src[2] * alpha + dst[2] * (255 - alpha)) / 255;
                        }
                    }
                }
            }
            SDL_FreeSurface(rgbSurface);
        }
        SDL_FreeSurface(surface);
    };

    // Draw shadow/outline (simple 1px offset)
    renderToBuffer(text, shadowColor, 1, 1);
    // Draw main text
    renderToBuffer(text, color, 0, 0);
}

void scaleRGB24(const uint8_t* src, int srcW, int srcH, uint8_t* dst, int dstW, int dstH) {
    if (!src || !dst) return;
    float xRatio = (float)srcW / (float)dstW;
    float yRatio = (float)srcH / (float)dstH;

    for (int i = 0; i < dstH; i++) {
        for (int j = 0; j < dstW; j++) {
            int px = (int)(j * xRatio);
            int py = (int)(i * yRatio);
            if (px >= srcW) px = srcW - 1;
            if (py >= srcH) py = srcH - 1;

            const uint8_t* srcP = src + (py * srcW + px) * 3;
            uint8_t* dstP = dst + (i * dstW + j) * 3;
            dstP[0] = srcP[0];
            dstP[1] = srcP[1];
            dstP[2] = srcP[2];
        }
    }
}

void rotateRGB24(const uint8_t* src, int srcW, int srcH, uint8_t* dst, int rotation) {
    if (!src || !dst) return;
    int rot = rotation % 360;
    if (rot < 0) rot += 360;

    int dstW = (rot == 90 || rot == 270) ? srcH : srcW;

    for (int y = 0; y < srcH; ++y) {
        for (int x = 0; x < srcW; ++x) {
            int nx, ny;
            if (rot == 90) { // CCW
                nx = y;
                ny = srcW - 1 - x;
            } else if (rot == 180) {
                nx = srcW - 1 - x;
                ny = srcH - 1 - y;
            } else if (rot == 270) {
                nx = srcH - 1 - y;
                ny = x;
            } else {
                nx = x;
                ny = y;
            }
            const uint8_t* srcP = src + (y * srcW + x) * 3;
            uint8_t* dstP = dst + (ny * dstW + nx) * 3;
            dstP[0] = srcP[0];
            dstP[1] = srcP[1];
            dstP[2] = srcP[2];
        }
    }
}

void rotateThermal(const uint16_t* src, int srcW, int srcH, uint16_t* dst, int rotation) {
    if (!src || !dst) return;
    int rot = rotation % 360;
    if (rot < 0) rot += 360;

    int dstW = (rot == 90 || rot == 270) ? srcH : srcW;

    for (int y = 0; y < srcH; ++y) {
        for (int x = 0; x < srcW; ++x) {
            int nx, ny;
            if (rot == 90) { // CCW
                nx = y;
                ny = srcW - 1 - x;
            } else if (rot == 180) {
                nx = srcW - 1 - x;
                ny = srcH - 1 - y;
            } else if (rot == 270) {
                nx = srcH - 1 - y;
                ny = x;
            } else {
                nx = x;
                ny = y;
            }
            dst[ny * dstW + nx] = src[y * srcW + x];
        }
    }
}

}
