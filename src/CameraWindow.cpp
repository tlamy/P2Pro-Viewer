#include "CameraWindow.hpp"
#include "P2Pro.hpp"
#include <iostream>
#include <cmath>

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
    float scale = (float)rw / (float)ww;

    if (scale > 1.0f) {
        dprintf("CameraWindow::init() - HiDPI detected (scale: %.1fx). Scaling initial window 2x...\n", scale);
        currentWidth = baseWidth * 2;
        currentHeight = baseHeight * 2;
        SDL_SetWindowSize(window, currentWidth, currentHeight);
    } else {
        dprintf("CameraWindow::init() - Standard DPI detected.\n");
        currentWidth = baseWidth;
        currentHeight = baseHeight;
    }

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

void CameraWindow::pollEvents(bool& running) {
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT) {
            running = false;
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
            }
        }
    }
}

void CameraWindow::updateFrame(const std::vector<uint8_t>& rgb_data, int w, int h) {
    if (w != baseWidth || h != baseHeight) return;
    SDL_UpdateTexture(texture, NULL, rgb_data.data(), w * 3);
}

void CameraWindow::render() {
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}
