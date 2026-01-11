#include "CameraWindow.hpp"
#include "P2Pro.hpp"
#include <iostream>
#include <cmath>

CameraWindow::CameraWindow(const std::string& title, int width, int height)
    : title(title), baseWidth(width), baseHeight(height), currentWidth(width), currentHeight(height),
      scaler(width, height) {
}

CameraWindow::~CameraWindow() {
    if (font) TTF_CloseFont(font);
    TTF_Quit();
    if (texture) SDL_DestroyTexture(texture);
    if (crosshairCursor) SDL_FreeCursor(crosshairCursor);
    if (defaultCursor) SDL_FreeCursor(defaultCursor);
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

    if (TTF_Init() == -1) {
        dprintf("SDL_ttf could not initialize! TTF_Error: %s\n", TTF_GetError());
        return false;
    }

    // Load font
    const char* fontPaths[] = {
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "Arial.ttf" // Fallback to current directory
    };

    for (const char* path : fontPaths) {
        font = TTF_OpenFont(path, 16);
        if (font) {
            dprintf("CameraWindow::init() - Loaded font: %s\n", path);
            break;
        }
    }

    if (!font) {
        dprintf("CameraWindow::init() - Warning: Could not load any font. Text rendering will be disabled.\n");
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

    // Set initial window size to 2x base resolution as requested
    dprintf("CameraWindow::init() - Setting initial window size to 2x base resolution.\n");
    currentWidth = baseWidth * 2;
    currentHeight = baseHeight * 2;
    SDL_SetWindowSize(window, currentWidth, currentHeight + toolbarHeight);

    SDL_RenderSetLogicalSize(renderer, currentWidth, currentHeight + toolbarHeight);
    SDL_ShowWindow(window);

    // Create cursors
    defaultCursor = SDL_GetDefaultCursor();
    crosshairCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);

    dprintf("CameraWindow::init() - Creating texture...\n");
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, baseWidth, baseHeight);
    if (!texture) {
        dprintf("Texture could not be created! SDL_Error: %s\n", SDL_GetError());
        return false;
    }

    SDL_SetWindowMinimumSize(window, baseWidth, baseHeight + toolbarHeight);

    dprintf("CameraWindow::init() - Success.\n");
    return true;
}

void CameraWindow::setRotation(int degrees) {
    int oldRotation = rotation;
    rotation = degrees % 360;

    // Update base dimensions based on rotation
    int origW = 256;
    int origH = 192;

    int oldBaseWidth = baseWidth;

    if (rotation == 90 || rotation == 270) {
        baseWidth = origH;
        baseHeight = origW;
    } else {
        baseWidth = origW;
        baseHeight = origH;
    }

    // Recreate texture with new dimensions
    if (texture) SDL_DestroyTexture(texture);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, baseWidth, baseHeight);

    // Update scaler
    scaler = Scaler(baseWidth, baseHeight);

    // Maintain current scale factor
    double currentScale = (double) currentWidth / oldBaseWidth;

    int steps = (int) std::round(std::log(currentScale) / (std::log(2.0) / 4.0));
    double targetScale = std::exp((std::log(2.0) / 4.0) * steps);

    currentWidth = (int) std::round(baseWidth * targetScale);
    currentHeight = (int) std::round(baseHeight * targetScale);

    SDL_SetWindowSize(window, currentWidth, currentHeight + toolbarHeight);
    SDL_RenderSetLogicalSize(renderer, currentWidth, currentHeight + toolbarHeight);
    SDL_SetWindowMinimumSize(window, baseWidth, baseHeight + toolbarHeight);
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
            int centerY = currentHeight + toolbarHeight - 40;
            int radius = 20;
            mouseOverButton = isPointInCircle(mouseX, mouseY, centerX, centerY, radius);
        } else if (e.type == SDL_MOUSEBUTTONDOWN) {
            if (e.button.button == SDL_BUTTON_LEFT) {
                if (mouseY < toolbarHeight) {
                    // Toolbar interaction
                    if (mouseX > 10 && mouseX < 50) {
                        showMouseTemp = !showMouseTemp;
                        SDL_SetCursor(showMouseTemp ? crosshairCursor : defaultCursor);
                    } else if (mouseX > 60 && mouseX < 100) {
                        setRotation((rotation + 90) % 360);
                    }
                } else if (mouseOverButton) {
                    recordToggleRequested = true;
                } else {
                    showMouseTemp = !showMouseTemp;
                    SDL_SetCursor(showMouseTemp ? crosshairCursor : defaultCursor);
                }
            }
        } else if (e.type == SDL_WINDOWEVENT) {
            if (e.window.event == SDL_WINDOWEVENT_RESIZED) {
                int newW = e.window.data1;
                int newH = e.window.data2 - toolbarHeight;

                int targetW, targetH;
                scaler.getScaledSize(newW, newH, targetW, targetH);

                if (newW != targetW || (e.window.data2 != targetH + toolbarHeight)) {
                    SDL_SetWindowSize(window, targetW, targetH + toolbarHeight);
                }
                currentWidth = targetW;
                currentHeight = targetH;
                SDL_RenderSetLogicalSize(renderer, currentWidth, currentHeight + toolbarHeight);
            }
        }
    }
}

void CameraWindow::updateFrame(const std::vector<uint8_t> &rgb_data, const std::vector<uint16_t> &thermal_data, int w,
                               int h) {
    if (rotation == 0) {
        if (w != 256 || h != 192) return;
        SDL_UpdateTexture(texture, NULL, rgb_data.data(), w * 3);
        currentThermal = thermal_data;
    } else {
        // Rotate
        int origW = 256;
        int origH = 192;
        std::vector<uint8_t> rotRGB(origW * origH * 3);
        std::vector<uint16_t> rotThermal(origW * origH);

        for (int y = 0; y < origH; ++y) {
            for (int x = 0; x < origW; ++x) {
                int nx, ny;
                if (rotation == 90) {
                    // Anti-clockwise
                    nx = y;
                    ny = origW - 1 - x;
                } else if (rotation == 180) {
                    nx = origW - 1 - x;
                    ny = origH - 1 - y;
                } else if (rotation == 270) {
                    nx = origH - 1 - y;
                    ny = x;
                } else {
                    nx = x;
                    ny = y;
                }

                int oldIdx = (y * origW + x);
                int newIdx = (ny * baseWidth + nx);

                rotRGB[newIdx * 3] = rgb_data[oldIdx * 3];
                rotRGB[newIdx * 3 + 1] = rgb_data[oldIdx * 3 + 1];
                rotRGB[newIdx * 3 + 2] = rgb_data[oldIdx * 3 + 2];
                rotThermal[newIdx] = thermal_data[oldIdx];
            }
        }
        SDL_UpdateTexture(texture, NULL, rotRGB.data(), baseWidth * 3);
        currentThermal = rotThermal;
    }
}

void CameraWindow::render(bool isRecording, bool indicatorVisible, bool isConnected, const HotSpotResult &hotSpot) {
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    SDL_RenderClear(renderer);

    renderToolbar();

    if (isConnected) {
        SDL_Rect viewport = {0, toolbarHeight, currentWidth, currentHeight};
        SDL_RenderCopy(renderer, texture, NULL, &viewport);

        // Adjust coordinate system for frame overlays
        // We'll wrap HotSpot and MouseTemp rendering to account for toolbarHeight
        
        renderHotSpot(hotSpot);

        if (showMouseTemp) {
            renderMouseTemp();
        }

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
    int centerY = currentHeight + toolbarHeight - 40;
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
    drawFilledCircle(renderer, padding + radius, toolbarHeight + padding + radius, radius);
}

void CameraWindow::renderMouseTemp() {
    if (currentThermal.empty() || !font) return;

    if (mouseY < toolbarHeight) return;

    // Scale from logical size to base dimensions
    float invScaleX = (float) baseWidth / (float) currentWidth;
    float invScaleY = (float) baseHeight / (float) currentHeight;

    int tx = (int) (mouseX * invScaleX);
    int ty = (int) ((mouseY - toolbarHeight) * invScaleY);

    if (tx < 0 || tx >= baseWidth || ty < 0 || ty >= baseHeight) return;

    uint16_t val = currentThermal[ty * baseWidth + tx];
    double tempC = (val / 64.0) - 273.15;

    char text[32];
    snprintf(text, sizeof(text), "%.1f C", tempC);

    SDL_Color white = {255, 255, 255, 255};
    SDL_Color black = {0, 0, 0, 255};

    SDL_Surface *surface = TTF_RenderText_Blended(font, text, white);
    if (surface) {
        SDL_Texture *msgTexture = SDL_CreateTextureFromSurface(renderer, surface);
        if (msgTexture) {
            int tooltipX = mouseX + 15;
            int tooltipY = mouseY - 25;

            // Background rect
            SDL_Rect bgRect = {tooltipX - 2, tooltipY - 2, surface->w + 4, surface->h + 4};
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_RenderFillRect(renderer, &bgRect);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

            SDL_Rect dstRect = {tooltipX, tooltipY, surface->w, surface->h};
            SDL_RenderCopy(renderer, msgTexture, NULL, &dstRect);
            SDL_DestroyTexture(msgTexture);
        }
        SDL_FreeSurface(surface);
    }
}

void CameraWindow::renderScanningMessage() {
    if (!font) return;
    std::string msg = "Searching for P2Pro camera...";
    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface* surface = TTF_RenderText_Blended(font, msg.c_str(), white);
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

    // The hotSpot.x/y are relative to the original sensor (256x192)
    // We need to rotate them if rotation is active
    int rx = hotSpot.x;
    int ry = hotSpot.y;
    int origW = 256;
    int origH = 192;

    if (rotation == 90) {
        rx = hotSpot.y;
        ry = origW - 1 - hotSpot.x;
    } else if (rotation == 180) {
        rx = origW - 1 - hotSpot.x;
        ry = origH - 1 - hotSpot.y;
    } else if (rotation == 270) {
        rx = origH - 1 - hotSpot.y;
        ry = hotSpot.x;
    }

    // Scale from base dimensions (rotated) to current logical size
    float scaleX = (float) currentWidth / (float) baseWidth;
    float scaleY = (float) currentHeight / (float) baseHeight;

    int x = (int) (rx * scaleX);
    int y = (int) (ry * scaleY) + toolbarHeight;

    // Safety check to avoid rendering outside the window/toolbar
    if (x < 0 || x >= currentWidth || y < toolbarHeight || y >= currentHeight + toolbarHeight) return;

    // Inverse color
    uint8_t invR = 255 - hotSpot.r;
    uint8_t invG = 255 - hotSpot.g;
    uint8_t invB = 255 - hotSpot.b;
    
    // Draw Crosshair (Inverse Color)
    SDL_SetRenderDrawColor(renderer, invR, invG, invB, 255);
    int crossSize = 12;
    SDL_RenderDrawLine(renderer, x - crossSize, y, x + crossSize, y);
    SDL_RenderDrawLine(renderer, x, y - crossSize, x, y + crossSize);

    // Render Text using SDL_ttf
    if (!font) return;
    char text[32];
    snprintf(text, sizeof(text), "%.1f C", hotSpot.tempC);

    // Contrast outline (hysteresis to prevent flickering)
    int brightness = hotSpot.r + hotSpot.g + hotSpot.b;
    if (darkOutline) {
        if (brightness < 300) darkOutline = false;
    } else {
        if (brightness > 450) darkOutline = true;
    }
    
    SDL_Color outlineColor = darkOutline ? SDL_Color{0, 0, 0, 255} : SDL_Color{255, 255, 255, 255};
    SDL_Color textColor = {invR, invG, invB, 255};

    // Render shadow/outline first
    SDL_Surface* shadowSurface = TTF_RenderText_Blended(font, text, outlineColor);
    if (shadowSurface) {
        SDL_Texture* shadowTexture = SDL_CreateTextureFromSurface(renderer, shadowSurface);
        if (shadowTexture) {
            SDL_Rect destRect = { x + 8 + 1, y - 8 - shadowSurface->h + 1, shadowSurface->w, shadowSurface->h };
            if (destRect.x + destRect.w > currentWidth) destRect.x = x - 8 - destRect.w + 1;
            if (destRect.y < toolbarHeight) destRect.y = y + 8 + 1;
            SDL_RenderCopy(renderer, shadowTexture, NULL, &destRect);
            SDL_DestroyTexture(shadowTexture);
        }

        // Render main text
        SDL_Surface* textSurface = TTF_RenderText_Blended(font, text, textColor);
        if (textSurface) {
            SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
            if (textTexture) {
                SDL_Rect destRect = { x + 8, y - 8 - textSurface->h, textSurface->w, textSurface->h };
                if (destRect.x + destRect.w > currentWidth) destRect.x = x - 8 - destRect.w;
                if (destRect.y < toolbarHeight) destRect.y = y + 8;
                SDL_RenderCopy(renderer, textTexture, NULL, &destRect);
                SDL_DestroyTexture(textTexture);
            }
            SDL_FreeSurface(textSurface);
        }
        SDL_FreeSurface(shadowSurface);
    }
}

void CameraWindow::renderToolbar() {
    // Toolbar background
    SDL_Rect tbRect = {0, 0, currentWidth, toolbarHeight};
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
    SDL_RenderFillRect(renderer, &tbRect);

    // Separator line
    SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
    SDL_RenderDrawLine(renderer, 0, toolbarHeight - 1, currentWidth, toolbarHeight - 1);

    // 1. Crosshair Icon (Temperature Tooltip Toggle)
    int chX = 30, chY = toolbarHeight / 2;
    int chSize = 10;
    SDL_SetRenderDrawColor(renderer, showMouseTemp ? 0 : 200, showMouseTemp ? 255 : 200, showMouseTemp ? 0 : 200, 255);
    SDL_RenderDrawLine(renderer, chX - chSize, chY, chX + chSize, chY);
    SDL_RenderDrawLine(renderer, chX, chY - chSize, chX, chY + chSize);
    // Outer circle for crosshair
    for (int i = 0; i < 360; i += 15) {
        float rad = i * M_PI / 180.0f;
        float rad2 = (i + 15) * M_PI / 180.0f;
        SDL_RenderDrawLine(renderer, chX + cos(rad) * 8, chY + sin(rad) * 8, chX + cos(rad2) * 8, chY + sin(rad2) * 8);
    }

    // 2. Rotate Icon (Anti-clockwise)
    int rotX = 80, rotY = toolbarHeight / 2;
    int rotSize = 8;
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    // Draw an arc (from 45 to 315 degrees, opening at the right)
    for (int i = 45; i < 315; i += 10) {
        float rad = i * M_PI / 180.0f;
        float rad2 = (i + 10) * M_PI / 180.0f;
        SDL_RenderDrawLine(renderer, rotX + cos(rad) * rotSize, rotY + sin(rad) * rotSize, rotX + cos(rad2) * rotSize,
                           rotY + sin(rad2) * rotSize);
    }
    // Arrow head at the lower opening (315 degrees)
    float arrowRad = 40 * M_PI / 180.0f;
    int ax = rotX + cos(arrowRad) * rotSize;
    int ay = rotY + sin(arrowRad) * rotSize;
    // Arrow pointing "left" and "up" to look like it continues the anti-clockwise motion
    // At 315 deg (top-right), pointing towards upper-left means negative x and negative y component relative to tangent
    SDL_RenderDrawLine(renderer, ax, ay, ax - 5, ay);
    SDL_RenderDrawLine(renderer, ax, ay, ax - 2, ay + 4);
}
