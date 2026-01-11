#include "Scaler.hpp"
#include <cmath>
#include <algorithm>

Scaler::Scaler(int baseWidth, int baseHeight)
    : baseW(baseWidth), baseH(baseHeight) {
    // We want a logarithmic approach where each step is an exponential increase.
    // Let's say 4 steps equals a doubling of size (2.0 scale).
    // scale = exp(k * steps)
    // 2.0 = exp(k * 4) => k = ln(2.0) / 4
    k = std::log(2.0) / 4.0;
}

void Scaler::getScaledSize(int inputW, int inputH, int &outputW, int &outputH) const {
    double scaleW = (double) inputW / baseW;
    double scaleH = (double) inputH / baseH;

    // Use the maximum of width/height scale to maintain aspect ratio and fit
    double scale = std::max(scaleW, scaleH);

    if (scale < 1.0) scale = 1.0;

    // scale = exp(k * steps) => steps = ln(scale) / k
    double stepsFloat = std::log(scale) / k;
    int steps = (int) std::round(stepsFloat);

    if (steps < 0) steps = 0;

    double targetScale = std::exp(k * steps);
    outputW = (int) std::round(baseW * targetScale);
    outputH = (int) std::round(baseH * targetScale);
}