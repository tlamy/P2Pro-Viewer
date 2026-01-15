#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <fstream>

void draw_circle(SDL_Surface* surface, int x, int y, int radius, SDL_Color color) {
    for (int w = 0; w < radius * 2; w++) {
        for (int h = 0; h < radius * 2; h++) {
            int dx = radius - w;
            int dy = radius - h;
            if ((dx * dx + dy * dy) <= (radius * radius)) {
                Uint32* pixels = (Uint32*)surface->pixels;
                if ((y + h - radius) >= 0 && (y + h - radius) < surface->h && (x + w - radius) >= 0 && (x + w - radius) < surface->w) {
                    pixels[(y + h - radius) * surface->w + (x + w - radius)] = SDL_MapRGBA(surface->format, color.r, color.g, color.b, color.a);
                }
            }
        }
    }
}

void render_p2_icon(int size, const std::string& fontPath) {
    // Use standard masks for 32-bit RGBA
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    Uint32 rmask = 0xff000000;
    Uint32 gmask = 0x00ff0000;
    Uint32 bmask = 0x0000ff00;
    Uint32 amask = 0x000000ff;
#else
    Uint32 rmask = 0x000000ff;
    Uint32 gmask = 0x0000ff00;
    Uint32 bmask = 0x00ff0000;
    Uint32 amask = 0xff000000;
#endif

    SDL_Surface* surface = SDL_CreateRGBSurface(0, size, size, 32, rmask, gmask, bmask, amask);
    if (!surface) return;

    // Fill with background (dark grey)
    SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, 40, 40, 40));

    // Dark grey circle
    SDL_Color darkGrey = {40, 40, 40, 255};
    draw_circle(surface, size / 2, size / 2, size / 2, darkGrey);

    // Load font - size should be roughly 60% of icon size
    TTF_Font* font = TTF_OpenFont(fontPath.c_str(), size * 0.6);
    if (!font) {
        std::cerr << "Failed to load font: " << fontPath << " for size " << size << std::endl;
        SDL_FreeSurface(surface);
        return;
    }

    // Rainbow colors
    SDL_Color colorP = {50, 200, 255, 255}; // Cyan
    SDL_Color color2 = {255, 200, 50, 255}; // Orange/Yellow

    SDL_Surface* surfP = TTF_RenderText_Blended(font, "P", colorP);
    SDL_Surface* surf2 = TTF_RenderText_Blended(font, "2", color2);

    if (surfP && surf2) {
        int totalWidth = surfP->w + surf2->w;
        SDL_Rect rectP = {(size - totalWidth) / 2, (size - surfP->h) / 2, surfP->w, surfP->h};
        SDL_Rect rect2 = {rectP.x + surfP->w, (size - surf2->h) / 2, surf2->w, surf2->h};

        SDL_BlitSurface(surfP, NULL, surface, &rectP);
        SDL_BlitSurface(surf2, NULL, surface, &rect2);
    }

    if (surfP) SDL_FreeSurface(surfP);
    if (surf2) SDL_FreeSurface(surf2);
    TTF_CloseFont(font);

    std::string filename = "icon_" + std::to_string(size) + ".bmp";
    SDL_SaveBMP(surface, filename.c_str());
    std::cout << "Generated " << filename << std::endl;

    SDL_FreeSurface(surface);
}

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return 1;
    if (TTF_Init() == -1) return 1;

    // System fonts on macOS
    const char* fonts[] = {
        "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "/Library/Fonts/Arial Bold.ttf"
    };

    std::string selectedFont = "";
    for (const char* f : fonts) {
        std::ifstream ifs(f);
        if (ifs.good()) {
            selectedFont = f;
            break;
        }
    }

    if (selectedFont.empty()) {
        std::cerr << "Could not find a suitable system font." << std::endl;
        return 1;
    }

    int sizes[] = {16, 32, 64, 128, 256, 512, 1024};
    for (int size : sizes) {
        render_p2_icon(size, selectedFont);
    }

    TTF_Quit();
    SDL_Quit();
    return 0;
}
