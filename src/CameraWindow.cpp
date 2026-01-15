#include "CameraWindow.hpp"
#include "P2Pro.hpp"
#include "Icons.hpp"
#include <iostream>
#include <cmath>

CameraWindow::CameraWindow(const std::string& title, int width, int height)
    : title(title), baseWidth(width), baseHeight(height), currentWidth(width), currentHeight(height),
      scaler(width, height) {
}

CameraWindow::~CameraWindow() {
    cleanupIcons();
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

    // Load fonts
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

    // Initialize icons
    initIcons();

    // Set initial window size
    currentWidth = (int)(baseWidth * currentScale);
    currentHeight = (int)(baseHeight * currentScale);
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

    SDL_SetWindowMinimumSize(window, (int)(baseWidth * 0.5f), (int)(baseHeight * 0.5f) + toolbarHeight);

    dprintf("CameraWindow::init() - Success.\n");
    return true;
}

void CameraWindow::setScale(float scale) {
    if (scale < 0.5f) scale = 0.5f;
    if (scale > 16.0f) scale = 16.0f;
    currentScale = scale;

    currentWidth = (int)(baseWidth * currentScale);
    currentHeight = (int)(baseHeight * currentScale);

    SDL_SetWindowSize(window, currentWidth, currentHeight + toolbarHeight);
    SDL_RenderSetLogicalSize(renderer, currentWidth, currentHeight + toolbarHeight);
}

float CameraWindow::getScale() const {
    return currentScale;
}

void CameraWindow::setRotation(int degrees) {
    rotation = degrees % 360;

    // Update base dimensions based on rotation
    int origW = 256;
    int origH = 192;

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
    currentWidth = (int) std::round(baseWidth * currentScale);
    currentHeight = (int) std::round(baseHeight * currentScale);

    SDL_SetWindowSize(window, currentWidth, currentHeight + toolbarHeight);
    SDL_RenderSetLogicalSize(renderer, currentWidth, currentHeight + toolbarHeight);
    SDL_SetWindowMinimumSize(window, (int)(baseWidth * 0.5f), (int)(baseHeight * 0.5f) + toolbarHeight);
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
            
            // Record button in toolbar is at x around 100
            mouseOverRecordButton = (mouseY < toolbarHeight && mouseX > 80 && mouseX < 120);
        } else if (e.type == SDL_MOUSEBUTTONDOWN) {
            if (e.button.button == SDL_BUTTON_LEFT) {
                if (mouseY < toolbarHeight) {
                    // Toolbar interaction
                    // Icon centers: 25, 65, 100, 135, 175, 215. Each hit area approx 35-40px.
                    if (mouseX >= 5 && mouseX < 45) { // Crosshair (center 25)
                        showMouseTemp = !showMouseTemp;
                        SDL_SetCursor(showMouseTemp ? crosshairCursor : defaultCursor);
                    } else if (mouseX >= 45 && mouseX < 85) { // Rotate CCW (center 65)
                        setRotation((rotation + 270) % 360);
                    } else if (mouseX >= 85 && mouseX < 120) { // Record (center 100)
                        recordToggleRequested = true;
                    } else if (mouseX >= 120 && mouseX < 155) { // Rotate CW (center 135)
                        setRotation((rotation + 90) % 360);
                    } else if (mouseX >= 155 && mouseX < 195) { // Zoom - (center 175)
                        float nextScale = currentScale;
                        if (currentScale > 1.0f) nextScale = std::floor(currentScale - 0.01f);
                        else if (currentScale > 0.5f) nextScale = 0.5f;
                        setScale(nextScale);
                    } else if (mouseX >= 195 && mouseX < 235) { // Zoom + (center 215)
                        float nextScale = currentScale;
                        if (currentScale < 1.0f) nextScale = 1.0f;
                        else if (currentScale < 16.0f) nextScale = std::ceil(currentScale + 0.01f);
                        setScale(nextScale);
                    }
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
                currentScale = (float)currentWidth / (float)baseWidth;
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

    renderToolbar(isRecording);

    if (isConnected) {
        SDL_Rect viewport = {0, toolbarHeight, currentWidth, currentHeight};
        SDL_RenderCopy(renderer, texture, NULL, &viewport);

        renderHotSpot(hotSpot);

        if (showMouseTemp) {
            renderMouseTemp();
        }

        if (isRecording && indicatorVisible) {
            renderIndicator();
        }
    } else {
        renderScanningMessage();
    }

    SDL_RenderPresent(renderer);
}

void CameraWindow::initIcons() {
    int w, h;
    
    // Check if we should use 2x icons (HiDPI)
    int drawableW, drawableH;
    SDL_GL_GetDrawableSize(window, &drawableW, &drawableH);
    int windowW, windowH;
    SDL_GetWindowSize(window, &windowW, &windowH);
    
    bool use2x = (drawableW > windowW);

    if (use2x) {
        iconCrosshair.texture = loadIconFromMemory(icon_Crosshair_48, icon_Crosshair_48_width, icon_Crosshair_48_height, icon_Crosshair_48_pitch);
        iconCrosshair.w = icon_Crosshair_48_width / 2; iconCrosshair.h = icon_Crosshair_48_height / 2;

        iconRotateCCW.texture = loadIconFromMemory(icon_RotateCCW_48, icon_RotateCCW_48_width, icon_RotateCCW_48_height, icon_RotateCCW_48_pitch);
        iconRotateCCW.w = icon_RotateCCW_48_width / 2; iconRotateCCW.h = icon_RotateCCW_48_height / 2;

        iconRotateCW.texture = loadIconFromMemory(icon_RotateCW_48, icon_RotateCW_48_width, icon_RotateCW_48_height, icon_RotateCW_48_pitch);
        iconRotateCW.w = icon_RotateCW_48_width / 2; iconRotateCW.h = icon_RotateCW_48_height / 2;

        iconRecord.texture = loadIconFromMemory(icon_Record_48, icon_Record_48_width, icon_Record_48_height, icon_Record_48_pitch);
        iconRecord.w = icon_Record_48_width / 2; iconRecord.h = icon_Record_48_height / 2;

        iconStop.texture = loadIconFromMemory(icon_Stop_48, icon_Stop_48_width, icon_Stop_48_height, icon_Stop_48_pitch);
        iconStop.w = icon_Stop_48_width / 2; iconStop.h = icon_Stop_48_height / 2;

        iconZoomIn.texture = loadIconFromMemory(icon_ZoomIn_48, icon_ZoomIn_48_width, icon_ZoomIn_48_height, icon_ZoomIn_48_pitch);
        iconZoomIn.w = icon_ZoomIn_48_width / 2; iconZoomIn.h = icon_ZoomIn_48_height / 2;

        iconZoomOut.texture = loadIconFromMemory(icon_ZoomOut_48, icon_ZoomOut_48_width, icon_ZoomOut_48_height, icon_ZoomOut_48_pitch);
        iconZoomOut.w = icon_ZoomOut_48_width / 2; iconZoomOut.h = icon_ZoomOut_48_height / 2;
    } else {
        iconCrosshair.texture = loadIconFromMemory(icon_Crosshair_24, icon_Crosshair_24_width, icon_Crosshair_24_height, icon_Crosshair_24_pitch);
        iconCrosshair.w = icon_Crosshair_24_width; iconCrosshair.h = icon_Crosshair_24_height;

        iconRotateCCW.texture = loadIconFromMemory(icon_RotateCCW_24, icon_RotateCCW_24_width, icon_RotateCCW_24_height, icon_RotateCCW_24_pitch);
        iconRotateCCW.w = icon_RotateCCW_24_width; iconRotateCCW.h = icon_RotateCCW_24_height;

        iconRotateCW.texture = loadIconFromMemory(icon_RotateCW_24, icon_RotateCW_24_width, icon_RotateCW_24_height, icon_RotateCW_24_pitch);
        iconRotateCW.w = icon_RotateCW_24_width; iconRotateCW.h = icon_RotateCW_24_height;

        iconRecord.texture = loadIconFromMemory(icon_Record_24, icon_Record_24_width, icon_Record_24_height, icon_Record_24_pitch);
        iconRecord.w = icon_Record_24_width; iconRecord.h = icon_Record_24_height;

        iconStop.texture = loadIconFromMemory(icon_Stop_24, icon_Stop_24_width, icon_Stop_24_height, icon_Stop_24_pitch);
        iconStop.w = icon_Stop_24_width; iconStop.h = icon_Stop_24_height;

        iconZoomIn.texture = loadIconFromMemory(icon_ZoomIn_24, icon_ZoomIn_24_width, icon_ZoomIn_24_height, icon_ZoomIn_24_pitch);
        iconZoomIn.w = icon_ZoomIn_24_width; iconZoomIn.h = icon_ZoomIn_24_height;

        iconZoomOut.texture = loadIconFromMemory(icon_ZoomOut_24, icon_ZoomOut_24_width, icon_ZoomOut_24_height, icon_ZoomOut_24_pitch);
        iconZoomOut.w = icon_ZoomOut_24_width; iconZoomOut.h = icon_ZoomOut_24_height;
    }
}

SDL_Texture* CameraWindow::loadIconFromMemory(const unsigned char* data, int width, int height, int pitch) {
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom((void*)data, width, height, 32, pitch, SDL_PIXELFORMAT_RGBA32);
    if (!surface) return nullptr;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return tex;
}

void CameraWindow::cleanupIcons() {
    SDL_DestroyTexture(iconCrosshair.texture);
    SDL_DestroyTexture(iconRotateCCW.texture);
    SDL_DestroyTexture(iconRotateCW.texture);
    SDL_DestroyTexture(iconRecord.texture);
    SDL_DestroyTexture(iconStop.texture);
    SDL_DestroyTexture(iconZoomIn.texture);
    SDL_DestroyTexture(iconZoomOut.texture);
    
    iconCrosshair.texture = nullptr;
    iconRotateCCW.texture = nullptr;
    iconRotateCW.texture = nullptr;
    iconRecord.texture = nullptr;
    iconStop.texture = nullptr;
    iconZoomIn.texture = nullptr;
    iconZoomOut.texture = nullptr;
}

void CameraWindow::renderToolbar(bool isRecording) {
    // Toolbar background
    SDL_Rect tbRect = {0, 0, currentWidth, toolbarHeight};
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
    SDL_RenderFillRect(renderer, &tbRect);

    // Separator line
    SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
    SDL_RenderDrawLine(renderer, 0, toolbarHeight - 1, currentWidth, toolbarHeight - 1);

    auto drawIcon = [&](IconTexture& icon, int x, bool active) {
        if (!icon.texture) return;
        SDL_Rect dest = { x - icon.w / 2, toolbarHeight / 2 - icon.h / 2, icon.w, icon.h };
        if (active) {
            SDL_SetTextureColorMod(icon.texture, 0, 255, 0);
        } else {
            SDL_SetTextureColorMod(icon.texture, 255, 255, 255);
        }
        SDL_RenderCopy(renderer, icon.texture, NULL, &dest);
    };

    drawIcon(iconCrosshair, 25, showMouseTemp);
    drawIcon(iconRotateCCW, 65, false);
    
    if (isRecording) {
        SDL_SetTextureColorMod(iconStop.texture, 255, 0, 0); // Red stop icon
        drawIcon(iconStop, 100, false);
    } else {
        drawIcon(iconRecord, 100, false);
    }

    drawIcon(iconRotateCW, 135, false);
    drawIcon(iconZoomOut, 175, false);
    drawIcon(iconZoomIn, 215, false);
    
    // Render current scale text
    if (font) {
        char scaleText[16];
        snprintf(scaleText, sizeof(scaleText), "%.0f%%", currentScale * 100.0f);
        SDL_Color white = {200, 200, 200, 255};
        SDL_Surface* surface = TTF_RenderText_Blended(font, scaleText, white);
        if (surface) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
            if (tex) {
                SDL_Rect dest = { 245, toolbarHeight / 2 - surface->h / 2, surface->w, surface->h };
                SDL_RenderCopy(renderer, tex, NULL, &dest);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surface);
        }
    }
}

void CameraWindow::renderIndicator() {
    int padding = 20;
    int radius = 8;
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    // Draw a simple red circle indicator
    for (int w = -radius; w <= radius; w++) {
        for (int h = -radius; h <= radius; h++) {
            if (w * w + h * h <= radius * radius) {
                SDL_RenderDrawPoint(renderer, padding + radius + w, toolbarHeight + padding + radius + h);
            }
        }
    }
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
            SDL_Rect dstRect = { (currentWidth - surface->w) / 2, (currentHeight - surface->h) / 2 + toolbarHeight, surface->w, surface->h };
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
