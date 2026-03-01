#ifndef COLOR_CONVERSION_HPP
#define COLOR_CONVERSION_HPP

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <vector>
#include <cstdint>

namespace ColorConversion {
    // Converts YUYV (4:2:2) to RGB
    void YUY2toRGB(const uint8_t* yuy2, uint8_t* rgb, int width, int height);
    
    // Converts RGB to BGR
    void RGBtoBGR(const uint8_t* rgb, uint8_t* bgr, int width, int height);

    // Converts RGB to BGR and flips vertically
    void RGBtoBGRFlipped(const uint8_t* rgb, uint8_t* bgr, int width, int height);

    // Draws a simple 5x7 bitmap character into an RGB buffer (24-bit)
    void drawChar(uint8_t* rgb, int width, int height, int x, int y, char c, uint8_t r, uint8_t g, uint8_t b, uint8_t br, uint8_t bg, uint8_t bb);

    // Draws a string using drawChar
    void drawText(uint8_t* rgb, int width, int height, int x, int y, const char* text, uint8_t r, uint8_t g, uint8_t b, uint8_t br, uint8_t bg, uint8_t bb);

    // Draws text using SDL_ttf into an RGB buffer (24-bit)
    void drawTTFText(uint8_t* rgb, int width, int height, int x, int y, const char* text, TTF_Font* font, SDL_Color color, SDL_Color shadowColor);

    // Simple nearest-neighbor scaling for RGB24
    void scaleRGB24(const uint8_t* src, int srcW, int srcH, uint8_t* dst, int dstW, int dstH);

    // Rotates RGB24 buffer (0, 90, 180, 270 degrees anti-clockwise)
    void rotateRGB24(const uint8_t* src, int srcW, int srcH, uint8_t* dst, int rotation);

    // Rotates Thermal buffer (0, 90, 180, 270 degrees anti-clockwise)
    void rotateThermal(const uint16_t* src, int srcW, int srcH, uint16_t* dst, int rotation);
}

#endif
