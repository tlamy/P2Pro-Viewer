#ifndef COLOR_CONVERSION_HPP
#define COLOR_CONVERSION_HPP

#include <vector>
#include <cstdint>

namespace ColorConversion {
    // Converts YUYV (4:2:2) to RGB
    void YUY2toRGB(const uint8_t* yuy2, uint8_t* rgb, int width, int height);
    
    // Converts RGB to BGR
    void RGBtoBGR(const uint8_t* rgb, uint8_t* bgr, int width, int height);
}

#endif
