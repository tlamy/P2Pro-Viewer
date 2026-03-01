#ifndef PREFERENCES_HPP
#define PREFERENCES_HPP

#include <string>
#include <vector>
#include <cstdint>
#include "ColorConversion.hpp"

class Preferences {
public:
    static Preferences& getInstance();

    // Prevent copying
    Preferences(const Preferences&) = delete;
    Preferences& operator=(const Preferences&) = delete;

    // Settings
    int windowX = -1; // -1 means center
    int windowY = -1;
    int windowW = 256;
    int windowH = 192;
    float zoom = 2.0f;
    int rotation = 0;
    std::string colorPaletteName = "GREYSCALE";
    float gamma = 1.0f;

    void load();
    void save();

    // Platform-specific preference directory
    static std::string getPrefsDir();

    ColorConversion::PaletteType paletteNameToEnum(const std::string& name);
    std::string paletteEnumToName(ColorConversion::PaletteType palette);

private:
    Preferences();
    ~Preferences();

    std::string getPrefsFilePath() const;
};

#endif
