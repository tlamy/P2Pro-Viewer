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

void applyPalette(const uint16_t* thermal, uint8_t* rgb, int width, int height, uint16_t minVal, uint16_t maxVal, PaletteType palette, float gamma) {
    int count = width * height;
    float range = (float)(maxVal - minVal);
    if (range <= 0) range = 1.0f;

    for (int i = 0; i < count; ++i) {
        float norm = (float)(thermal[i] - minVal) / range;
        norm = std::max(0.0f, std::min(1.0f, norm));

        if (gamma != 1.0f) {
            norm = std::pow(norm, gamma);
        }

        uint8_t r, g, b;
        switch (palette) {
            case PaletteType::GREYSCALE:
                r = g = b = (uint8_t)(norm * 255.0f);
                break;
            case PaletteType::HOTNESS: {
                // black - blue - red - yellow - white
                if (norm < 0.25f) { // black to blue
                    r = 0; g = 0; b = (uint8_t)(norm * 4.0f * 255.0f);
                } else if (norm < 0.5f) { // blue to red
                    r = (uint8_t)((norm - 0.25f) * 4.0f * 255.0f); g = 0; b = (uint8_t)(255 - (norm - 0.25f) * 4.0f * 255.0f);
                } else if (norm < 0.75f) { // red to yellow
                    r = 255; g = (uint8_t)((norm - 0.5f) * 4.0f * 255.0f); b = 0;
                } else { // yellow to white
                    r = 255; g = 255; b = (uint8_t)((norm - 0.75f) * 4.0f * 255.0f);
                }
                break;
            }
            case PaletteType::RAINBOW: {
                // Rainbow (cold blue to hot red)
                // We use hue from 240 (blue) down to 0 (red)
                float h = (1.0f - norm) * 240.0f; 
                float s = 1.0f, v = 1.0f;
                float c = v * s;
                float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
                float m = v - c;
                float r_, g_, b_;
                if (h < 60) { r_ = c; g_ = x; b_ = 0; }
                else if (h < 120) { r_ = x; g_ = c; b_ = 0; }
                else if (h < 180) { r_ = 0; g_ = c; b_ = x; }
                else if (h < 240) { r_ = 0; g_ = x; b_ = c; }
                else { r_ = c; g_ = 0; b_ = x; }
                r = (uint8_t)((r_ + m) * 255.0f);
                g = (uint8_t)((g_ + m) * 255.0f);
                b = (uint8_t)((b_ + m) * 255.0f);
                break;
            }
            case PaletteType::INVERSE_RAINBOW: {
                // Inverse Rainbow (cold red to hot blue)
                // We use hue from 0 (red) up to 240 (blue)
                float h = norm * 240.0f;
                float s = 1.0f, v = 1.0f;
                float c = v * s;
                float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
                float m = v - c;
                float r_, g_, b_;
                if (h < 60) { r_ = c; g_ = x; b_ = 0; }
                else if (h < 120) { r_ = x; g_ = c; b_ = 0; }
                else if (h < 180) { r_ = 0; g_ = c; b_ = x; }
                else if (h < 240) { r_ = 0; g_ = x; b_ = c; }
                else { r_ = c; g_ = 0; b_ = x; }
                r = (uint8_t)((r_ + m) * 255.0f);
                g = (uint8_t)((g_ + m) * 255.0f);
                b = (uint8_t)((b_ + m) * 255.0f);
                break;
            }
            default:
                r = g = b = 0;
        }
        rgb[i * 3] = r;
        rgb[i * 3 + 1] = g;
        rgb[i * 3 + 2] = b;
    }
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

// Special case for side-by-side dev images (each 256x192)
void rotateDevRGB24(const uint8_t* src, uint8_t* dst, int rotation) {
    if (!src || !dst) return;
    int rot = rotation % 360;
    if (rot < 0) rot += 360;

    if (rot == 0) {
        memcpy(dst, src, 512 * 192 * 3);
        return;
    }

    int dstW = (rot == 90 || rot == 270) ? 192 : 256;
    int dstEffectiveW = dstW * 2;

    // First half (Original)
    std::vector<uint8_t> tmp(256 * 192 * 3);
    for (int y = 0; y < 192; ++y) {
        memcpy(tmp.data() + y * 256 * 3, src + (y * 512 + 0) * 3, 256 * 3);
    }
    
    for (int y = 0; y < 192; ++y) {
        for (int x = 0; x < 256; ++x) {
            int nx, ny;
            if (rot == 90) { nx = y; ny = 256 - 1 - x; }
            else if (rot == 180) { nx = 256 - 1 - x; ny = 192 - 1 - y; }
            else if (rot == 270) { nx = 192 - 1 - y; ny = x; }
            else { nx = x; ny = y; }
            const uint8_t* srcP = tmp.data() + (y * 256 + x) * 3;
            uint8_t* dstP = dst + (ny * dstEffectiveW + nx) * 3;
            dstP[0] = srcP[0]; dstP[1] = srcP[1]; dstP[2] = srcP[2];
        }
    }

    // Second half (Custom)
    for (int y = 0; y < 192; ++y) {
        memcpy(tmp.data() + y * 256 * 3, src + (y * 512 + 256) * 3, 256 * 3);
    }
    
    for (int y = 0; y < 192; ++y) {
        for (int x = 0; x < 256; ++x) {
            int nx, ny;
            if (rot == 90) { nx = y; ny = 256 - 1 - x; }
            else if (rot == 180) { nx = 256 - 1 - x; ny = 192 - 1 - y; }
            else if (rot == 270) { nx = 192 - 1 - y; ny = x; }
            else { nx = x; ny = y; }
            const uint8_t* srcP = tmp.data() + (y * 256 + x) * 3;
            uint8_t* dstP = dst + (ny * dstEffectiveW + (nx + dstW)) * 3;
            dstP[0] = srcP[0]; dstP[1] = srcP[1]; dstP[2] = srcP[2];
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
