#ifndef MF_VIDEO_SOURCE_HPP
#define MF_VIDEO_SOURCE_HPP

#include <vector>
#include <cstdint>
#include <string>

class MFVideoSource {
public:
    MFVideoSource();
    ~MFVideoSource();

    bool open(int width, int height);
    void close();
    bool isOpened() const;

    bool getFrame(std::vector<uint8_t>& frameData);

private:
    void* impl; // Opaque pointer to implementation
};

#endif
