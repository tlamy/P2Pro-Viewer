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
    {"Stop", "\xee\x99\x87"}, // Use 'square' instead of 'stop' to get a filled square
    {"ZoomIn", "\xee\xa3\xbf"},
    {"ZoomOut", "\xee\xa4\x80"},
    {"ArrowUpward", "\xee\x97\x98"},
    {"ArrowDownward", "\xee\x97\x9b"}
};

void export_icon(TTF_Font* font, const IconInfo& icon, int size, std::ofstream& out) {
    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, icon.codepoint, white);
    if (!surface) {
        std::cerr << "Failed to render icon: " << icon.name << " at size " << size << " Error: " << TTF_GetError() << std::endl;
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

int main(int argc, char* argv[]) {
    std::cout << "Starting IconGen (No-SDL version)..." << std::endl;
    if (TTF_Init() == -1) {
        std::cerr << "TTF_Init Error: " << TTF_GetError() << std::endl;
        return 1;
    }

    const char* fontPath = "MaterialIcons-Regular.ttf";
    
    std::cout << "Writing to Icons_new.hpp..." << std::endl;
    std::ofstream out("Icons_new.hpp");
    if (!out.is_open()) {
        std::cerr << "Failed to open Icons_new.hpp for writing" << std::endl;
        return 1;
    }
    out << "#ifndef ICONS_HPP" << std::endl;
    out << "#define ICONS_HPP" << std::endl << std::endl;

    int sizes[] = {24, 48};
    for (int size : sizes) {
        std::cout << "Loading font " << fontPath << " at size " << size << "..." << std::endl;
        TTF_Font* font = TTF_OpenFont(fontPath, size);
        if (!font) {
            std::cerr << "Failed to load font at size " << size << ": " << TTF_GetError() << std::endl;
            continue;
        }
        for (const auto& icon : icons) {
            std::cout << "Exporting icon: " << icon.name << " (" << size << ")..." << std::endl;
            export_icon(font, icon, size, out);
        }
        TTF_CloseFont(font);
    }

    out << "#endif" << std::endl;
    out.close();

    std::cout << "Done." << std::endl;
    TTF_Quit();
    return 0;
}
