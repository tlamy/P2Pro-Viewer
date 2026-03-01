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

static const uint8_t font5x7[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, // (space)
    0x00, 0x00, 0x5F, 0x00, 0x00, // !
    0x00, 0x07, 0x00, 0x07, 0x00, // "
    0x14, 0x7F, 0x14, 0x7F, 0x14, // #
    0x24, 0x2A, 0x7F, 0x2A, 0x12, // $
    0x23, 0x13, 0x08, 0x64, 0x62, // %
    0x36, 0x49, 0x55, 0x22, 0x50, // &
    0x00, 0x05, 0x03, 0x00, 0x00, // '
    0x00, 0x1C, 0x22, 0x41, 0x00, // (
    0x00, 0x41, 0x22, 0x1C, 0x00, // )
    0x08, 0x2A, 0x1C, 0x2A, 0x08, // *
    0x08, 0x08, 0x3E, 0x08, 0x08, // +
    0x00, 0x50, 0x30, 0x00, 0x00, // ,
    0x08, 0x08, 0x08, 0x08, 0x08, // -
    0x00, 0x60, 0x60, 0x00, 0x00, // .
    0x20, 0x10, 0x08, 0x04, 0x02, // /
    0x3E, 0x51, 0x49, 0x45, 0x3E, // 0
    0x00, 0x42, 0x7F, 0x40, 0x00, // 1
    0x42, 0x61, 0x51, 0x49, 0x46, // 2
    0x21, 0x41, 0x45, 0x4B, 0x31, // 3
    0x18, 0x14, 0x12, 0x7F, 0x10, // 4
    0x27, 0x45, 0x45, 0x45, 0x39, // 5
    0x3C, 0x4A, 0x49, 0x49, 0x30, // 6
    0x01, 0x71, 0x09, 0x05, 0x03, // 7
    0x36, 0x49, 0x49, 0x49, 0x36, // 8
    0x06, 0x49, 0x49, 0x29, 0x1E, // 9
    0x00, 0x36, 0x36, 0x00, 0x00, // :
    0x00, 0x56, 0x36, 0x00, 0x00, // ;
    0x00, 0x08, 0x14, 0x22, 0x41, // <
    0x14, 0x14, 0x14, 0x14, 0x14, // =
    0x41, 0x22, 0x14, 0x08, 0x00, // >
    0x02, 0x01, 0x51, 0x09, 0x06, // ?
    0x32, 0x49, 0x79, 0x41, 0x3E, // @
    0x7E, 0x11, 0x11, 0x11, 0x7E, // A
    0x7F, 0x49, 0x49, 0x49, 0x36, // B
    0x3E, 0x41, 0x41, 0x41, 0x22, // C
    0x7F, 0x41, 0x41, 0x22, 0x1C, // D
    0x7F, 0x49, 0x49, 0x49, 0x41, // E
    0x7F, 0x09, 0x09, 0x01, 0x01, // F
    0x3E, 0x41, 0x41, 0x51, 0x32, // G
    0x7F, 0x08, 0x08, 0x08, 0x7F, // H
    0x00, 0x41, 0x7F, 0x41, 0x00, // I
    0x20, 0x40, 0x41, 0x3F, 0x01, // J
    0x7F, 0x08, 0x14, 0x22, 0x41, // K
    0x7F, 0x40, 0x40, 0x40, 0x40, // L
    0x7F, 0x02, 0x04, 0x02, 0x7F, // M
    0x7F, 0x04, 0x08, 0x10, 0x7F, // N
    0x3E, 0x41, 0x41, 0x41, 0x3E, // O
    0x7F, 0x09, 0x09, 0x09, 0x06, // P
    0x3E, 0x41, 0x51, 0x21, 0x5E, // Q
    0x7F, 0x09, 0x19, 0x29, 0x46, // R
    0x46, 0x49, 0x49, 0x49, 0x31, // S
    0x01, 0x01, 0x7F, 0x01, 0x01, // T
    0x3F, 0x40, 0x40, 0x40, 0x3F, // U
    0x1F, 0x20, 0x40, 0x20, 0x1F, // V
    0x7F, 0x20, 0x10, 0x20, 0x7F, // W
    0x63, 0x14, 0x08, 0x14, 0x63, // X
    0x03, 0x04, 0x78, 0x04, 0x03, // Y
    0x61, 0x51, 0x49, 0x45, 0x43, // Z
    0x00, 0x7F, 0x41, 0x41, 0x00, // [
    0x02, 0x04, 0x08, 0x10, 0x20, // (back slash)
    0x00, 0x41, 0x41, 0x7F, 0x00, // ]
    0x04, 0x02, 0x01, 0x02, 0x04, // ^
    0x40, 0x40, 0x40, 0x40, 0x40, // _
    0x00, 0x01, 0x02, 0x04, 0x00, // `
    0x20, 0x54, 0x54, 0x54, 0x78, // a
    0x7F, 0x48, 0x44, 0x44, 0x38, // b
    0x38, 0x44, 0x44, 0x44, 0x20, // c
    0x38, 0x44, 0x44, 0x48, 0x7F, // d
    0x38, 0x54, 0x54, 0x54, 0x18, // e
    0x08, 0x7E, 0x09, 0x01, 0x02, // f
    0x08, 0x14, 0x54, 0x54, 0x3C, // g
    0x7F, 0x08, 0x04, 0x04, 0x78, // h
    0x00, 0x44, 0x7D, 0x40, 0x00, // i
    0x20, 0x40, 0x44, 0x3D, 0x00, // j
    0x7F, 0x10, 0x28, 0x44, 0x00, // k
    0x00, 0x41, 0x7F, 0x40, 0x00, // l
    0x7C, 0x04, 0x18, 0x04, 0x78, // m
    0x7C, 0x08, 0x04, 0x04, 0x78, // n
    0x38, 0x44, 0x44, 0x44, 0x38, // o
    0x7C, 0x14, 0x14, 0x14, 0x08, // p
    0x08, 0x14, 0x14, 0x18, 0x7C, // q
    0x7C, 0x08, 0x04, 0x04, 0x08, // r
    0x48, 0x54, 0x54, 0x54, 0x20, // s
    0x04, 0x3F, 0x44, 0x40, 0x20, // t
    0x3C, 0x40, 0x40, 0x20, 0x7C, // u
    0x1C, 0x20, 0x40, 0x20, 0x1C, // v
    0x3C, 0x40, 0x30, 0x40, 0x3C, // w
    0x44, 0x28, 0x10, 0x28, 0x44, // x
    0x0C, 0x50, 0x50, 0x50, 0x3C, // y
    0x44, 0x64, 0x54, 0x4C, 0x44, // z
    0x00, 0x08, 0x36, 0x41, 0x00, // {
    0x00, 0x00, 0x7F, 0x00, 0x00, // |
    0x00, 0x41, 0x36, 0x08, 0x00, // }
    0x08, 0x08, 0x2A, 0x1C, 0x08 // ~
};

void drawChar(uint8_t* rgb, int width, int height, int x, int y, char c, uint8_t r, uint8_t g, uint8_t b, uint8_t br, uint8_t bg, uint8_t bb) {
    if (c < 32 || c > 126) c = '?';
    int font_idx = (c - 32) * 5;
    for (int col = 0; col < 5; ++col) {
        uint8_t bits = font5x7[font_idx + col];
        for (int row = 0; row < 7; ++row) {
            int px = x + col;
            int py = y + row;
            if (px >= 0 && px < width && py >= 0 && py < height) {
                int idx = (py * width + px) * 3;
                if (bits & (1 << row)) {
                    rgb[idx] = r;
                    rgb[idx + 1] = g;
                    rgb[idx + 2] = b;
                } else if (br != 0 || bg != 0 || bb != 0 || r == 0 && g == 0 && b == 0) {
                     // Only draw background if it's not transparent (approx by checking if it's not pure black unless foreground is black)
                     // But for simple implementation, we can just not draw background to keep it transparent.
                }
            }
        }
    }
}

void drawText(uint8_t* rgb, int width, int height, int x, int y, const char* text, uint8_t r, uint8_t g, uint8_t b, uint8_t br, uint8_t bg, uint8_t bb) {
    int curX = x;
    while (*text) {
        // Draw background shadow for better visibility
        drawChar(rgb, width, height, curX + 1, y + 1, *text, br, bg, bb, 0, 0, 0);
        drawChar(rgb, width, height, curX, y, *text, r, g, b, 0, 0, 0);
        curX += 6;
        text++;
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
