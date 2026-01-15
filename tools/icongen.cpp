#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <iomanip>
#include <string>

struct IconInfo {
    const char* name;
    const char* codepoint;
};

IconInfo icons[] = {
    {"Crosshair", "\xee\x8e\xb8"},
    {"RotateCCW", "\xee\x90\x99"},
    {"RotateCW", "\xee\x90\x9a"},
    {"Record", "\xee\x81\xa1"},
    {"Stop", "\xee\x80\x87"},
    {"ZoomIn", "\xee\xa3\xbf"},
    {"ZoomOut", "\xee\xa4\x80"}
};

void export_icon(SDL_Renderer* renderer, TTF_Font* font, const IconInfo& icon, int size, std::ofstream& out) {
    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, icon.codepoint, white);
    if (!surface) {
        std::cerr << "Failed to render icon: " << icon.name << " at size " << size << std::endl;
        return;
    }

    out << "const unsigned char icon_" << icon.name << "_" << size << "[] = {" << std::endl;
    unsigned char* pixels = (unsigned char*)surface->pixels;
    for (int i = 0; i < surface->h * surface->pitch; ++i) {
        out << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)pixels[i] << ", ";
        if ((i + 1) % 16 == 0) out << std::endl;
    }
    out << std::dec << "};" << std::endl;
    out << "const int icon_" << icon.name << "_" << size << "_width = " << surface->w << ";" << std::endl;
    out << "const int icon_" << icon.name << "_" << size << "_height = " << surface->h << ";" << std::endl;
    out << "const int icon_" << icon.name << "_" << size << "_pitch = " << surface->pitch << ";" << std::endl << std::endl;

    SDL_FreeSurface(surface);
}

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return 1;
    if (TTF_Init() == -1) return 1;

    SDL_Window* window = SDL_CreateWindow("IconGen", 0, 0, 100, 100, SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);

    const char* fontPath = "MaterialIcons-Regular.ttf";
    
    std::ofstream out("src/Icons.hpp");
    out << "#ifndef ICONS_HPP" << std::endl;
    out << "#define ICONS_HPP" << std::endl << std::endl;

    int sizes[] = {24, 48};
    for (int size : sizes) {
        TTF_Font* font = TTF_OpenFont(fontPath, size);
        if (!font) {
            std::cerr << "Failed to load font at size " << size << std::endl;
            continue;
        }
        for (const auto& icon : icons) {
            export_icon(renderer, font, icon, size, out);
        }
        TTF_CloseFont(font);
    }

    out << "#endif" << std::endl;

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
