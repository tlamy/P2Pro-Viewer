#include "CameraWindow.hpp"
#include "P2Pro.hpp"
#include "Icons.hpp"
#include "ColorConversion.hpp"
#include "Preferences.hpp"
#include <iostream>
#include <cmath>

CameraWindow::CameraWindow(const std::string& title, int width, int height)
    : title(title), baseWidth(width), baseHeight(height), currentWidth(width), currentHeight(height),
      scaler(width, height) {
}

CameraWindow::~CameraWindow() {
    cleanupIcons();
    if (font) TTF_CloseFont(font);
    if (recordingFont) TTF_CloseFont(recordingFont);
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
        "C:/Windows/Fonts/arial.ttf",                        // Windows
        "C:/Windows/Fonts/segoeui.ttf",                      // Windows fallback
        "/System/Library/Fonts/Supplemental/Arial.ttf",      // macOS
        "/System/Library/Fonts/Helvetica.ttc",               // macOS fallback
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",   // Linux
        "Arial.ttf" // Fallback to current directory
    };

    for (const char* path : fontPaths) {
        font = TTF_OpenFont(path, 16);
        recordingFont = TTF_OpenFont(path, 16); // Revert recording font back to same size as standard font
        if (font) {
            dprintf("CameraWindow::init() - Loaded font: %s (size 16)\n", path);
            break;
        }
    }

    if (!font) {
        dprintf("CameraWindow::init() - Warning: Could not load any font. Text rendering will be disabled.\n");
    }

    dprintf("CameraWindow::init() - Creating window...\n");
    Preferences& prefs = Preferences::getInstance();
    
    // Check if preferences need to be used
    int winX = prefs.windowX == -1 ? SDL_WINDOWPOS_CENTERED : prefs.windowX;
    int winY = prefs.windowY == -1 ? SDL_WINDOWPOS_CENTERED : prefs.windowY;
    
    // Use stored scale
    currentScale = prefs.zoom;
    rotation = prefs.rotation;
    palette = prefs.paletteNameToEnum(prefs.colorPaletteName);
    gamma = prefs.gamma;

    // Update dimensions based on rotation and palette
    int origW = 256;
    int origH = 192;
    if (rotation == 90 || rotation == 270) {
        baseWidth = origH;
        baseHeight = origW;
    } else {
        baseWidth = origW;
        baseHeight = origH;
    }
    
    currentWidth = (int)(baseWidth * currentScale);
    currentHeight = (int)(baseHeight * currentScale);

    window = SDL_CreateWindow(title.c_str(), winX, winY, currentWidth, currentHeight + toolbarHeight, SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
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

    Preferences& prefs = Preferences::getInstance();
    prefs.zoom = currentScale;
    prefs.save();

    int origW = 256;
    int origH = 192;
    int curBaseW, curBaseH;
    if (rotation == 90 || rotation == 270) {
        curBaseW = origH; curBaseH = origW;
    } else {
        curBaseW = origW; curBaseH = origH;
    }

    int effectiveBaseWidth = devMode ? curBaseW * 2 : curBaseW;

    // We must ensure the texture matches current devMode state
    if (texture) {
        int tw, th;
        SDL_QueryTexture(texture, NULL, NULL, &tw, &th);
        if (tw != effectiveBaseWidth || th != curBaseH) {
            SDL_DestroyTexture(texture);
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, effectiveBaseWidth, curBaseH);
            scaler = Scaler(effectiveBaseWidth, curBaseH);
        }
    }

    currentWidth = (int)(effectiveBaseWidth * currentScale);
    currentHeight = (int)(curBaseH * currentScale);

    SDL_SetWindowSize(window, currentWidth, currentHeight + toolbarHeight);
    SDL_RenderSetLogicalSize(renderer, currentWidth, currentHeight + toolbarHeight);
    SDL_SetWindowMinimumSize(window, (int)(effectiveBaseWidth * 0.5f), (int)(curBaseH * 0.5f) + toolbarHeight);
}

float CameraWindow::getScale() const {
    return currentScale;
}

void CameraWindow::setRotation(int degrees) {
    rotation = degrees % 360;

    Preferences& prefs = Preferences::getInstance();
    prefs.rotation = rotation;
    prefs.save();

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

    int effectiveBaseWidth = devMode ? baseWidth * 2 : baseWidth;

    // Recreate texture with new dimensions
    if (texture) SDL_DestroyTexture(texture);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, effectiveBaseWidth, baseHeight);

    // Update scaler
    scaler = Scaler(effectiveBaseWidth, baseHeight);

    // Maintain current scale factor
    currentWidth = (int) std::round(effectiveBaseWidth * currentScale);
    currentHeight = (int) std::round(baseHeight * currentScale);

    SDL_SetWindowSize(window, currentWidth, currentHeight + toolbarHeight);
    SDL_RenderSetLogicalSize(renderer, currentWidth, currentHeight + toolbarHeight);
    SDL_SetWindowMinimumSize(window, (int)(effectiveBaseWidth * 0.5f), (int)(baseHeight * 0.5f) + toolbarHeight);
}

void CameraWindow::setPalette(ColorConversion::PaletteType p) {
    palette = p;
    Preferences& prefs = Preferences::getInstance();
    prefs.colorPaletteName = prefs.paletteEnumToName(palette);
    prefs.save();
}

void CameraWindow::setGamma(float g) {
    gamma = g;
    if (gamma < 0.1f) gamma = 0.1f;
    if (gamma > 5.0f) gamma = 5.0f;
    Preferences& prefs = Preferences::getInstance();
    prefs.gamma = gamma;
    prefs.save();
}

void CameraWindow::pollEvents(bool& running, bool& recordToggleRequested, bool isRecording) {
    SDL_Event e;
    recordToggleRequested = false;
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT) {
            running = false;
        } else if (e.type == SDL_MOUSEMOTION) {
            mouseX = e.motion.x;
            mouseY = e.motion.y;
            
            // Icons centers: 25, 65, 100, 135, 175, 215. Each hit area approx 35-40px.
            mouseOverRecordButton = (mouseY < toolbarHeight && mouseX >= 80 && mouseX < 120);
            mouseOverPaletteButton = (mouseY < toolbarHeight && mouseX >= 280 && mouseX < 325);
            // Gamma display starts at 345. Down arrow is at 332. Up arrow is at 345 + textW + 18.
            // "G: X.X" is about 45-50px wide. 345 + 50 + 18 = 413.
            mouseOverGammaDownButton = (mouseY < toolbarHeight && mouseX >= 315 && mouseX < 342);
            mouseOverGammaUpButton = (mouseY < toolbarHeight && mouseX >= 400 && mouseX < 430);
            mouseOverDevModeButton = (mouseY < toolbarHeight && mouseX >= 430 && mouseX < 475);
        } else if (e.type == SDL_MOUSEBUTTONDOWN) {
            if (e.button.button == SDL_BUTTON_LEFT) {
                if (mouseY < toolbarHeight) {
                    // Toolbar interaction
                    // Icon centers: 25, 65, 100, 135, 175, 215. Each hit area approx 35-40px.
                    if (mouseX >= 5 && mouseX < 45) { // Crosshair (center 25)
                        showMouseTemp = !showMouseTemp;
                        SDL_SetCursor(showMouseTemp ? crosshairCursor : defaultCursor);
                    } else if (mouseX >= 45 && mouseX < 85) { // Rotate CCW (center 65)
                        if (!isRecording) setRotation((rotation + 270) % 360);
                    } else if (mouseX >= 85 && mouseX < 120) { // Record (center 100)
                        recordToggleRequested = true;
                    } else if (mouseX >= 120 && mouseX < 155) { // Rotate CW (center 135)
                        if (!isRecording) setRotation((rotation + 90) % 360);
                    } else if (mouseX >= 155 && mouseX < 195) { // Zoom - (center 175)
                        if (!isRecording) {
                            float nextScale = currentScale;
                            if (currentScale > 1.0f) nextScale = std::floor(currentScale - 0.01f);
                            else if (currentScale > 0.5f) nextScale = 0.5f;
                            else if (currentScale > 0.25f) nextScale = 0.25f;
                            else if (currentScale > 0.125f) nextScale = 0.125f;
                            setScale(nextScale);
                        }
                    } else if (mouseX >= 195 && mouseX < 235) { // Zoom + (center 215)
                        if (!isRecording) {
                            float nextScale = currentScale;
                            if (currentScale < 0.25f) nextScale = 0.25f;
                            else if (currentScale < 0.5f) nextScale = 0.5f;
                            else if (currentScale < 1.0f) nextScale = 1.0f;
                            else if (currentScale < 16.0f) nextScale = std::ceil(currentScale + 0.01f);
                            setScale(nextScale);
                        }
                    } else if (mouseX >= 280 && mouseX < 325) { // Palette
                        int nextPalette = (int)palette + 1;
                        if (nextPalette > (int)ColorConversion::PaletteType::INVERSE_RAINBOW) nextPalette = 0;
                        setPalette((ColorConversion::PaletteType)nextPalette);
                    } else if (mouseX >= 315 && mouseX < 342) { // Gamma Down
                        setGamma(gamma - 0.1f);
                    } else if (mouseX >= 400 && mouseX < 430) { // Gamma Up
                        setGamma(gamma + 0.1f);
                    } else if (mouseX >= 430 && mouseX < 475) { // Dev Mode
                        devMode = !devMode;
                        // Trigger resize for side-by-side
                        setScale(currentScale);
                    }
                } else {
                    showMouseTemp = !showMouseTemp;
                    SDL_SetCursor(showMouseTemp ? crosshairCursor : defaultCursor);
                }
            }
        } else if (e.type == SDL_WINDOWEVENT) {
            if (e.window.event == SDL_WINDOWEVENT_RESIZED) {
                if (isRecording) {
                    // Ignore resize when recording
                    SDL_SetWindowSize(window, currentWidth, currentHeight + toolbarHeight);
                } else {
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

                    Preferences& prefs = Preferences::getInstance();
                    prefs.zoom = currentScale;
                    prefs.windowW = targetW;
                    prefs.windowH = targetH + toolbarHeight;
                    prefs.save();
                }
            } else if (e.window.event == SDL_WINDOWEVENT_MOVED) {
                Preferences& prefs = Preferences::getInstance();
                prefs.windowX = e.window.data1;
                prefs.windowY = e.window.data2;
                prefs.save();
            }
        }
    }
}

void CameraWindow::updateFrame(const std::vector<uint8_t> &rgb_data, const std::vector<uint16_t> &thermal_data, int w,
                               int h) {
    if (rotation == 0) {
        if (w != 256 || h != 192) return;
        int effectiveW = devMode ? w * 2 : w;
        SDL_UpdateTexture(texture, NULL, rgb_data.data(), effectiveW * 3);
        currentThermal = thermal_data;
    } else {
        int effectiveW = devMode ? w * 2 : w;
        std::vector<uint8_t> rotRGB(effectiveW * h * 3);
        std::vector<uint16_t> rotThermal(w * h);
        
        if (devMode) {
            ColorConversion::rotateDevRGB24(rgb_data.data(), rotRGB.data(), rotation);
        } else {
            ColorConversion::rotateRGB24(rgb_data.data(), w, h, rotRGB.data(), rotation);
        }
        ColorConversion::rotateThermal(thermal_data.data(), w, h, rotThermal.data(), rotation);

        SDL_UpdateTexture(texture, NULL, rotRGB.data(), (devMode ? baseWidth * 2 : baseWidth) * 3);
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

        iconArrowUp.texture = loadIconFromMemory(icon_ArrowUpward_48, icon_ArrowUpward_48_width, icon_ArrowUpward_48_height, icon_ArrowUpward_48_pitch);
        iconArrowUp.w = icon_ArrowUpward_48_width / 2; iconArrowUp.h = icon_ArrowUpward_48_height / 2;

        iconArrowDown.texture = loadIconFromMemory(icon_ArrowDownward_48, icon_ArrowDownward_48_width, icon_ArrowDownward_48_height, icon_ArrowDownward_48_pitch);
        iconArrowDown.w = icon_ArrowDownward_48_width / 2; iconArrowDown.h = icon_ArrowDownward_48_height / 2;
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

        iconArrowUp.texture = loadIconFromMemory(icon_ArrowUpward_24, icon_ArrowUpward_24_width, icon_ArrowUpward_24_height, icon_ArrowUpward_24_pitch);
        iconArrowUp.w = icon_ArrowUpward_24_width; iconArrowUp.h = icon_ArrowUpward_24_height;

        iconArrowDown.texture = loadIconFromMemory(icon_ArrowDownward_24, icon_ArrowDownward_24_width, icon_ArrowDownward_24_height, icon_ArrowDownward_24_pitch);
        iconArrowDown.w = icon_ArrowDownward_24_width; iconArrowDown.h = icon_ArrowDownward_24_height;
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
    SDL_DestroyTexture(iconArrowUp.texture);
    SDL_DestroyTexture(iconArrowDown.texture);
    
    iconCrosshair.texture = nullptr;
    iconRotateCCW.texture = nullptr;
    iconRotateCW.texture = nullptr;
    iconRecord.texture = nullptr;
    iconStop.texture = nullptr;
    iconZoomIn.texture = nullptr;
    iconZoomOut.texture = nullptr;
    iconArrowUp.texture = nullptr;
    iconArrowDown.texture = nullptr;
}

void CameraWindow::renderToolbar(bool isRecording) {
    // Toolbar background
    SDL_Rect tbRect = {0, 0, currentWidth, toolbarHeight};
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
    SDL_RenderFillRect(renderer, &tbRect);

    // Separator line
    SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
    SDL_RenderDrawLine(renderer, 0, toolbarHeight - 1, currentWidth, toolbarHeight - 1);

    auto drawIcon = [&](IconTexture& icon, int x, bool active, bool disabled = false) {
        if (!icon.texture) return;
        SDL_Rect dest = { x - icon.w / 2, toolbarHeight / 2 - icon.h / 2, icon.w, icon.h };
        if (disabled) {
            SDL_SetTextureColorMod(icon.texture, 80, 80, 80); // Grayed out
        } else if (active) {
            SDL_SetTextureColorMod(icon.texture, 0, 255, 0);
        } else {
            SDL_SetTextureColorMod(icon.texture, 255, 255, 255);
        }
        SDL_RenderCopy(renderer, icon.texture, NULL, &dest);
    };

    drawIcon(iconCrosshair, 25, showMouseTemp);
    drawIcon(iconRotateCCW, 65, false, isRecording);
    
    if (isRecording) {
        SDL_SetTextureColorMod(iconStop.texture, 255, 0, 0); // Red stop icon
        drawIcon(iconStop, 100, false);
    } else {
        drawIcon(iconRecord, 100, false);
    }

    drawIcon(iconRotateCW, 135, false, isRecording);
    drawIcon(iconZoomOut, 175, false, isRecording);
    drawIcon(iconZoomIn, 215, false, isRecording);

    // Palette button
    if (font) {
        const char* paletteNames[] = {"Grey", "Hot", "Rb1", "Rb2"};
        const char* pName = paletteNames[(int)palette];
        SDL_Color white = {200, 200, 200, 255};
        if (mouseOverPaletteButton) white = {0, 255, 0, 255};
        SDL_Surface* surface = TTF_RenderText_Blended(font, pName, white);
        if (surface) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
            if (tex) {
                SDL_Rect dest = { 280, toolbarHeight / 2 - surface->h / 2, surface->w, surface->h };
                SDL_RenderCopy(renderer, tex, NULL, &dest);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surface);
        }
    }

    // Gamma button and arrows
    if (font) {
        char gammaText[32];
        snprintf(gammaText, sizeof(gammaText), "G: %.1f", gamma);
        SDL_Color white = {200, 200, 200, 255};
        SDL_Surface* surface = TTF_RenderText_Blended(font, gammaText, white);
        if (surface) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
            if (tex) {
                SDL_Rect dest = { 345, toolbarHeight / 2 - surface->h / 2, surface->w, surface->h };
                SDL_RenderCopy(renderer, tex, NULL, &dest);
                SDL_DestroyTexture(tex);
                
                // Arrows next to gamma text
                // Down arrow (left of text)
                drawIcon(iconArrowDown, 332, mouseOverGammaDownButton);
                // Up arrow (right of text)
                drawIcon(iconArrowUp, 345 + surface->w + 18, mouseOverGammaUpButton);
            }
            SDL_FreeSurface(surface);
        }
    }

    // DevMode button
    if (font) {
        SDL_Color green = {0, 255, 0, 255};
        SDL_Color grey = {200, 200, 200, 255};
        SDL_Color cyan = {0, 255, 255, 255};
        SDL_Color textColor = devMode ? green : grey;
        if (mouseOverDevModeButton) textColor = cyan;
        SDL_Surface* surface = TTF_RenderText_Blended(font, "DEV", textColor);
        if (surface) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
            if (tex) {
                SDL_Rect dest = { 430, toolbarHeight / 2 - surface->h / 2, surface->w, surface->h };
                SDL_RenderCopy(renderer, tex, NULL, &dest);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surface);
        }
    }
    
    // Render current scale text
    if (font) {
        char scaleText[32];
        if (std::abs(currentScale - 1.0f) < 0.01f) {
            snprintf(scaleText, sizeof(scaleText), "1");
        } else if (currentScale > 1.0f) {
            snprintf(scaleText, sizeof(scaleText), "x%.0f", std::round(currentScale));
        } else {
            snprintf(scaleText, sizeof(scaleText), "÷%.0f", std::round(1.0f / currentScale));
        }
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

    int sensorW = (rotation == 90 || rotation == 270) ? 192 : 256;
    int sensorH = (rotation == 90 || rotation == 270) ? 256 : 192;

    // Scale from logical size to sensor dimensions
    float scale = (float) currentHeight / (float) sensorH;

    int tx = (int) (mouseX / scale);
    int ty = (int) ((mouseY - toolbarHeight) / scale);

    if (devMode) {
        // Left half is original, right half is custom.
        // Both represent the same sensor area.
        if (tx >= sensorW) {
            tx -= sensorW;
        }
    }

    if (tx < 0 || tx >= sensorW || ty < 0 || ty >= sensorH) return;

    uint16_t val = currentThermal[ty * sensorW + tx];
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
    // In devMode, baseWidth is 2 * sensorWidth, and currentWidth is baseWidth * scale.
    // We want the hotspot on the custom colorized image (right half).
    
    int sensorW = (rotation == 90 || rotation == 270) ? 192 : 256;
    int sensorH = (rotation == 90 || rotation == 270) ? 256 : 192;

    float scale = (float) currentHeight / (float) sensorH;

    int x = (int) (rx * scale);
    int y = (int) (ry * scale) + toolbarHeight;

    // Hot-spot should work on the new custom-colorized image always.
    // In devMode, we show hotspot on the right half (custom color).
    // In normal mode, we only show custom color, so we don't need to offset.
    if (devMode) {
        x += (int)(sensorW * scale);
    }

    // Safety check to avoid rendering outside the window/toolbar
    if (x < 0 || x >= currentWidth || y < toolbarHeight || y >= currentHeight + toolbarHeight) return;

    // Use high-contrast colors: White with Black shadow/outline
    SDL_Color textColor = {255, 255, 255, 255};
    SDL_Color outlineColor = {0, 0, 0, 255};
    
    // Draw Crosshair Shadow
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    int crossSize = 12;
    SDL_RenderDrawLine(renderer, x - crossSize + 1, y + 1, x + crossSize + 1, y + 1);
    SDL_RenderDrawLine(renderer, x + 1, y - crossSize + 1, x + 1, y + crossSize + 1);

    // Draw Crosshair (White)
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawLine(renderer, x - crossSize, y, x + crossSize, y);
    SDL_RenderDrawLine(renderer, x, y - crossSize, x, y + crossSize);

    // Render Text using SDL_ttf
    if (!font) return;
    char text[32];
    snprintf(text, sizeof(text), "%.1f C", hotSpot.tempC);

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
