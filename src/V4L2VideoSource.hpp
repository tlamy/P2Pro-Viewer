#ifndef V4L2_VIDEO_SOURCE_HPP
#define V4L2_VIDEO_SOURCE_HPP

#include <string>
#include <vector>
#include <cstdint>

class V4L2VideoSource {
public:
    V4L2VideoSource();
    ~V4L2VideoSource();

    bool open(const std::string& device, int width, int height);
    void close();
    bool isOpened() const { return fd != -1; }

    bool getFrame(std::vector<uint8_t>& frameData);

private:
    int fd = -1;
    int width = 0;
    int height = 0;

    struct Buffer {
        void* start;
        size_t length;
    };
    std::vector<Buffer> buffers;

    bool init_mmap();
};

#endif
