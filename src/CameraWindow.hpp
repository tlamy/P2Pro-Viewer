#ifndef CAMERA_WINDOW_HPP
#define CAMERA_WINDOW_HPP

#include <SDL2/SDL.h>
#include <string>
#include <vector>
#include <cstdint>

class CameraWindow {
public:
    CameraWindow(const std::string& title, int width, int height);
    ~CameraWindow();

    bool init();
    void pollEvents(bool& running);
    void updateFrame(const std::vector<uint8_t>& rgb_data, int w, int h);
    void render();

private:
    std::string title;
    int baseWidth;
    int baseHeight;
    int currentWidth;
    int currentHeight;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
};

#endif
