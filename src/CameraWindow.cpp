#include "CameraWindow.hpp"
#include "P2Pro.hpp"
#include <iostream>
#include <cmath>
#include <opencv2/opencv.hpp>

CameraWindow::CameraWindow(const std::string& title, int width, int height)
    : title(title), baseWidth(width), baseHeight(height), currentWidth(width), currentHeight(height) {}

CameraWindow::~CameraWindow() {
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}

bool CameraWindow::init() {
    dprintf("CameraWindow::init() - Initializing SDL...\n");
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        dprintf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return false;
    }

    dprintf("CameraWindow::init() - Creating window...\n");
    window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, baseWidth, baseHeight, SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        dprintf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return false;
    }

    dprintf("CameraWindow::init() - Creating renderer...\n");
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        dprintf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return false;
    }

    // Detect HiDPI scale
    int ww, wh, rw, rh;
    SDL_GetWindowSize(window, &ww, &wh);
    SDL_GetRendererOutputSize(renderer, &rw, &rh);
    dpiScale = (float)rw / (float)ww;

    if (dpiScale > 1.0f) {
        dprintf("CameraWindow::init() - HiDPI detected (scale: %.1fx). Scaling initial window 2x...\n", dpiScale);
        currentWidth = baseWidth * 2;
        currentHeight = baseHeight * 2;
        SDL_SetWindowSize(window, currentWidth, currentHeight);
    } else {
        dprintf("CameraWindow::init() - Standard DPI detected.\n");
        currentWidth = baseWidth;
        currentHeight = baseHeight;
    }

    SDL_RenderSetLogicalSize(renderer, currentWidth, currentHeight);
    SDL_ShowWindow(window);

    dprintf("CameraWindow::init() - Creating texture...\n");
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, baseWidth, baseHeight);
    if (!texture) {
        dprintf("Texture could not be created! SDL_Error: %s\n", SDL_GetError());
        return false;
    }

    SDL_SetWindowMinimumSize(window, baseWidth, baseHeight);

    dprintf("CameraWindow::init() - Success.\n");
    return true;
}

void CameraWindow::pollEvents(bool& running, bool& recordToggleRequested) {
    SDL_Event e;
    recordToggleRequested = false;
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT) {
            running = false;
        } else if (e.type == SDL_MOUSEMOTION) {
            mouseX = e.motion.x;
            mouseY = e.motion.y;

            // Check if mouse is over button area (bottom center)
            int centerX = currentWidth / 2;
            int centerY = currentHeight - 40;
            int radius = 20;
            mouseOverButton = isPointInCircle(mouseX, mouseY, centerX, centerY, radius);
        } else if (e.type == SDL_MOUSEBUTTONDOWN) {
            if (e.button.button == SDL_BUTTON_LEFT && mouseOverButton) {
                recordToggleRequested = true;
            }
        } else if (e.type == SDL_WINDOWEVENT) {
            if (e.window.event == SDL_WINDOWEVENT_RESIZED) {
                int newW = e.window.data1;
                int newH = e.window.data2;

                // Steps of 25% of base size
                float stepW = baseWidth * 0.25f;
                float stepH = baseHeight * 0.25f;

                // Calculate how many steps away from base size we are
                int stepsW = (int)std::round((float)(newW - baseWidth) / stepW);
                int stepsH = (int)std::round((float)(newH - baseHeight) / stepH);
                int steps = std::max(stepsW, stepsH);
                if (steps < 0) steps = 0;

                int targetW = baseWidth + (int)(steps * stepW);
                int targetH = baseHeight + (int)(steps * stepH);

                if (newW != targetW || newH != targetH) {
                    SDL_SetWindowSize(window, targetW, targetH);
                }
                currentWidth = targetW;
                currentHeight = targetH;
                SDL_RenderSetLogicalSize(renderer, currentWidth, currentHeight);
            }
        }
    }
}

void CameraWindow::updateFrame(const std::vector<uint8_t>& rgb_data, int w, int h) {
    if (w != baseWidth || h != baseHeight) return;
    SDL_UpdateTexture(texture, NULL, rgb_data.data(), w * 3);
}

void CameraWindow::render(bool isRecording, bool indicatorVisible, bool isConnected, const HotSpotResult& hotSpot) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    
    if (isConnected) {
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        renderHotSpot(hotSpot);

        if (mouseOverButton) {
            renderRecordButton(isRecording);
        }

        if (isRecording && indicatorVisible) {
            renderIndicator();
        }
    } else {
        renderScanningMessage();
    }

    SDL_RenderPresent(renderer);
}

bool CameraWindow::isPointInCircle(int px, int py, int cx, int cy, int radius) {
    int dx = px - cx;
    int dy = py - cy;
    return (dx * dx + dy * dy) <= (radius * radius);
}

static void drawFilledCircle(SDL_Renderer* renderer, int x, int y, int radius) {
    for (int w = -radius; w <= radius; w++) {
        for (int h = -radius; h <= radius; h++) {
            if (w * w + h * h <= radius * radius) {
                SDL_RenderDrawPoint(renderer, x + w, y + h);
            }
        }
    }
}

void CameraWindow::renderRecordButton(bool isRecording) {
    int centerX = currentWidth / 2;
    int centerY = currentHeight - 40;
    int outerRadius = 20;
    int innerRadius = 16;

    // Draw white/light grey border (outer circle)
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    drawFilledCircle(renderer, centerX, centerY, outerRadius);

    if (!isRecording) {
        // Standby: Red circle
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        drawFilledCircle(renderer, centerX, centerY, innerRadius);
    } else {
        // Recording: Black square
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_Rect rect = { centerX - 10, centerY - 10, 20, 20 };
        SDL_RenderFillRect(renderer, &rect);
    }
}

void CameraWindow::renderIndicator() {
    int padding = 20;
    int radius = 8;
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    drawFilledCircle(renderer, padding + radius, padding + radius, radius);
}

void CameraWindow::renderScanningMessage() {
    std::string msg = "Searching for P2Pro camera...";
    
    double fontScale = 0.5;
    int baseLine;
    cv::Size textSize = cv::getTextSize(msg, cv::FONT_HERSHEY_SIMPLEX, fontScale, 1, &baseLine);
    
    cv::Mat textMat = cv::Mat::zeros(textSize.height + baseLine + 4, textSize.width + 4, CV_8UC4);
    cv::putText(textMat, msg, cv::Point(2, textSize.height + 2), cv::FONT_HERSHEY_SIMPLEX, fontScale, cv::Scalar(255, 255, 255, 255), 1, cv::LINE_AA);
    
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(textMat.data, textMat.cols, textMat.rows, 32, textMat.cols * 4, SDL_PIXELFORMAT_BGRA32);
    if (surface) {
        SDL_Texture* msgTexture = SDL_CreateTextureFromSurface(renderer, surface);
        if (msgTexture) {
            SDL_Rect dstRect = { (currentWidth - surface->w) / 2, (currentHeight - surface->h) / 2, surface->w, surface->h };
            SDL_RenderCopy(renderer, msgTexture, NULL, &dstRect);
            SDL_DestroyTexture(msgTexture);
        }
        SDL_FreeSurface(surface);
    }
}

void CameraWindow::renderHotSpot(const HotSpotResult& hotSpot) {
    if (!hotSpot.found) return;

    // Scale from 256x192 to current logical size
    float scaleX = (float)currentWidth / 256.0f;
    float scaleY = (float)currentHeight / 192.0f;

    int x = (int)(hotSpot.x * scaleX);
    int y = (int)(hotSpot.y * scaleY);

    // Inverse color
    uint8_t invR = 255 - hotSpot.r;
    uint8_t invG = 255 - hotSpot.g;
    uint8_t invB = 255 - hotSpot.b;

    // Draw Crosshair (Inverse Color)
    SDL_SetRenderDrawColor(renderer, invR, invG, invB, 255);
    int crossSize = 12;
    SDL_RenderDrawLine(renderer, x - crossSize, y, x + crossSize, y);
    SDL_RenderDrawLine(renderer, x, y - crossSize, x, y + crossSize);

    // Render Text using OpenCV to generate a temporary texture
    char text[32];
    snprintf(text, sizeof(text), "%.1f C", hotSpot.tempC);

    double fontScale = 0.6; // Increased font scale
    int baseLine;
    cv::Size textSize = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, fontScale, 1, &baseLine);
    
    // Create a small Mat for the text
    cv::Mat textMat = cv::Mat::zeros(textSize.height + baseLine + 4, textSize.width + 4, CV_8UC4);
    
    // Contrast outline (hysteresis to prevent flickering)
    int brightness = hotSpot.r + hotSpot.g + hotSpot.b;
    if (darkOutline) {
        if (brightness < 300) darkOutline = false;
    } else {
        if (brightness > 450) darkOutline = true;
    }
    cv::Scalar outlineColor = darkOutline ? cv::Scalar(0, 0, 0, 255) : cv::Scalar(255, 255, 255, 255);
    
    cv::putText(textMat, text, cv::Point(2, textSize.height + 2) + cv::Point(1, 1), cv::FONT_HERSHEY_SIMPLEX, fontScale, outlineColor, 1, cv::LINE_AA);
    // Inverse color text (Scalar is BGRA for this Mat/Surface combo)
    cv::putText(textMat, text, cv::Point(2, textSize.height + 2), cv::FONT_HERSHEY_SIMPLEX, fontScale, cv::Scalar(invB, invG, invR, 255), 1, cv::LINE_AA);

    // Convert to SDL_Texture
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(textMat.data, textMat.cols, textMat.rows, 32, textMat.cols * 4, SDL_PIXELFORMAT_BGRA32);
    if (surface) {
        SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, surface);
        if (textTexture) {
            SDL_Rect destRect = { x + 8, y - 8 - textSize.height, textMat.cols, textMat.rows };
            if (destRect.x + destRect.w > currentWidth) destRect.x = x - 8 - destRect.w;
            if (destRect.y < 0) destRect.y = y + 8;

            SDL_RenderCopy(renderer, textTexture, NULL, &destRect);
            SDL_DestroyTexture(textTexture);
        }
        SDL_FreeSurface(surface);
    }
}
