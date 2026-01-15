#ifndef CAMERA_WINDOW_HPP
#define CAMERA_WINDOW_HPP

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>
#include "P2Pro.hpp"
#include "Scaler.hpp"

class CameraWindow {
public:
    CameraWindow(const std::string &title, int width, int height);

    ~CameraWindow();

    bool init();

    void pollEvents(bool &running, bool &recordToggleRequested);

    void updateFrame(const std::vector<uint8_t> &rgb_data, const std::vector<uint16_t> &thermal_data, int w, int h);

    void render(bool isRecording, bool indicatorVisible, bool isConnected, const HotSpotResult &hotSpot = {});

    void setRotation(int degrees); // 0, 90, 180, 270
    void setScale(float scale);
    float getScale() const;

private:
    std::string title;
    int baseWidth;
    int baseHeight;
    int currentWidth;
    int currentHeight;
    int toolbarHeight = 40;
    int rotation = 0; // 0, 90, 180, 270 degrees anti-clockwise
    float currentScale = 2.0f;
    int mouseX = 0;
    int mouseY = 0;
    bool mouseOverRecordButton = false;
    bool showMouseTemp = false;
    std::vector<uint16_t> currentThermal;
    bool darkOutline = true;
    bool isScanning = false;
    Scaler scaler;

    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_Texture *texture = nullptr;
    TTF_Font *font = nullptr;
    SDL_Cursor *crosshairCursor = nullptr;
    SDL_Cursor *defaultCursor = nullptr;

    struct IconTexture {
        SDL_Texture* texture = nullptr;
        int w = 0;
        int h = 0;
    };
    IconTexture iconCrosshair;
    IconTexture iconRotateCCW;
    IconTexture iconRotateCW;
    IconTexture iconRecord;
    IconTexture iconStop;
    IconTexture iconZoomIn;
    IconTexture iconZoomOut;

    bool isPointInCircle(int px, int py, int cx, int cy, int radius);
    void renderIndicator();
    void renderHotSpot(const HotSpotResult &hotSpot);
    void renderMouseTemp();
    void renderToolbar(bool isRecording);
    void renderScanningMessage();
    
    void cleanupIcons();
    void initIcons();
    SDL_Texture* loadIconFromMemory(const unsigned char* data, int width, int height, int pitch);
};

#endif