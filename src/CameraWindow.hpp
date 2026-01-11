#ifndef CAMERA_WINDOW_HPP
#define CAMERA_WINDOW_HPP

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>
#include "P2Pro.hpp"

class CameraWindow {
public:
    CameraWindow(const std::string& title, int width, int height);
    ~CameraWindow();

    bool init();
    void pollEvents(bool& running, bool& recordToggleRequested);
    void updateFrame(const std::vector<uint8_t>& rgb_data, int w, int h);
    void render(bool isRecording, bool indicatorVisible, bool isConnected, const HotSpotResult& hotSpot = {});

private:
    std::string title;
    int baseWidth;
    int baseHeight;
    int currentWidth;
    int currentHeight;
    float dpiScale = 1.0f;
    int mouseX = 0;
    int mouseY = 0;
    bool mouseOverButton = false;
    bool darkOutline = true;
    bool isScanning = false;

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    TTF_Font* font = nullptr;

    bool isPointInCircle(int px, int py, int cx, int cy, int radius);
    void renderRecordButton(bool isRecording);
    void renderIndicator();
    void renderHotSpot(const HotSpotResult& hotSpot);
    void renderScanningMessage();
};

#endif
