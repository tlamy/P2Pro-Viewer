#ifndef AVFOUNDATION_VIDEO_SOURCE_HPP
#define AVFOUNDATION_VIDEO_SOURCE_HPP

#include <vector>
#include <cstdint>
#include <string>

class AVFoundationVideoSource {
public:
    AVFoundationVideoSource();
    ~AVFoundationVideoSource();

    // Returns a list of available camera names
    static std::vector<std::string> listDevices();

    // Opens a camera by its index or name
    bool open(int index, int width, int height, int fps);
    bool openByName(const std::string& name, int width, int height, int fps);
    
    void close();
    bool isOpened() const;

    // Copies the latest frame into the provided vector (YUYV format expected)
    bool getFrame(std::vector<uint8_t>& frameData);

private:
    void* impl; // Opaque pointer to the Objective-C implementation class
};

#endif
