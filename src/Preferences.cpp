#include "Preferences.hpp"
#include "P2Pro.hpp" // For dprintf
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <map>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <cstdlib>
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

Preferences& Preferences::getInstance() {
    static Preferences instance;
    return instance;
}

Preferences::Preferences() {
    load();
}

Preferences::~Preferences() {
    save();
}

std::string Preferences::getPrefsDir() {
    std::string path;
#ifdef __APPLE__
    const char* home = std::getenv("HOME");
    if (home) {
        path = std::string(home) + "/Library/Preferences";
    }
#elif defined(_WIN32)
    PWSTR wpath = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &wpath))) {
        int len = WideCharToMultiByte(CP_UTF8, 0, wpath, -1, NULL, 0, NULL, NULL);
        if (len > 0) {
            std::vector<char> buf(len);
            WideCharToMultiByte(CP_UTF8, 0, wpath, -1, buf.data(), len, NULL, NULL);
            path = buf.data();
            path += "\\P2ProViewer";
        }
        CoTaskMemFree(wpath);
    }
#else // Linux/Other
    const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
    if (xdgConfig) {
        path = xdgConfig;
    } else {
        const char* home = std::getenv("HOME");
        if (home) {
            path = std::string(home) + "/.config";
        }
    }
    if (!path.empty()) {
        path += "/P2ProViewer";
    }
#endif

    // Ensure directory exists
    if (!path.empty()) {
#ifdef _WIN32
        CreateDirectoryA(path.c_str(), NULL);
#else
        mkdir(path.c_str(), 0755);
#endif
    }
    return path;
}

std::string Preferences::getPrefsFilePath() const {
    std::string dir = getPrefsDir();
    if (dir.empty()) return "preferences.txt";
#ifdef _WIN32
    return dir + "\\preferences.txt";
#else
    return dir + "/com.p2proviewer.prefs.txt";
#endif
}

void Preferences::load() {
    std::string filePath = getPrefsFilePath();
    std::ifstream file(filePath);
    if (!file.is_open()) {
        dprintf("Preferences::load() - Could not open prefs file: %s (using defaults)\n", filePath.c_str());
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string key;
        if (!(iss >> key)) continue;
        if (key == "windowX") iss >> windowX;
        else if (key == "windowY") iss >> windowY;
        else if (key == "windowW") iss >> windowW;
        else if (key == "windowH") iss >> windowH;
        else if (key == "zoom") iss >> zoom;
        else if (key == "rotation") iss >> rotation;
        else if (key == "palette") iss >> colorPaletteName;
        else if (key == "gamma") iss >> gamma;
    }
    dprintf("Preferences::load() - Loaded from %s\n", filePath.c_str());
}

void Preferences::save() {
    std::string filePath = getPrefsFilePath();
    // dprintf("Preferences::save() - Saving to %s\n", filePath.c_str());
    std::ofstream file(filePath);
    if (!file.is_open()) {
        // dprintf("Preferences::save() - Could not open prefs file for writing: %s\n", filePath.c_str());
        return;
    }

    file << "windowX " << windowX << "\n";
    file << "windowY " << windowY << "\n";
    file << "windowW " << windowW << "\n";
    file << "windowH " << windowH << "\n";
    file << "zoom " << zoom << "\n";
    file << "rotation " << rotation << "\n";
    file << "palette " << colorPaletteName << "\n";
    file << "gamma " << gamma << "\n";
    file.flush();
    file.close();
}

ColorConversion::PaletteType Preferences::paletteNameToEnum(const std::string& name) {
    if (name == "HOTNESS") return ColorConversion::PaletteType::HOTNESS;
    if (name == "RAINBOW") return ColorConversion::PaletteType::RAINBOW;
    if (name == "INVERSE_RAINBOW") return ColorConversion::PaletteType::INVERSE_RAINBOW;
    return ColorConversion::PaletteType::GREYSCALE;
}

std::string Preferences::paletteEnumToName(ColorConversion::PaletteType palette) {
    switch (palette) {
        case ColorConversion::PaletteType::HOTNESS: return "HOTNESS";
        case ColorConversion::PaletteType::RAINBOW: return "RAINBOW";
        case ColorConversion::PaletteType::INVERSE_RAINBOW: return "INVERSE_RAINBOW";
        default: return "GREYSCALE";
    }
}
