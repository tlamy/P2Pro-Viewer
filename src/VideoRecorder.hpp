#ifndef VIDEO_RECORDER_HPP
#define VIDEO_RECORDER_HPP

#include <string>
#include <vector>
#include <cstdint>

class VideoRecorder {
public:
    VideoRecorder();
    ~VideoRecorder();

    bool start(int width, int height, double fps);
    void stop();
    void writeFrame(const std::vector<uint8_t>& rgb_data);

    bool isRecording() const { return recording; }
    std::string getFilename() const { return filename; }

private:
    bool recording = false;
    std::string filename;
    int width = 0;
    int height = 0;
    double fps = 0;
    int64_t frame_count = 0;

    void* impl = nullptr;

    std::string generateFilename() const;
    void cleanup();
};

#endif
