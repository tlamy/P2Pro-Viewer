#ifndef VIDEO_RECORDER_HPP
#define VIDEO_RECORDER_HPP

#include <string>
#include <vector>
#include <cstdint>

// Forward declarations for FFmpeg structures
struct AVFormatContext;
struct AVStream;
struct AVCodecContext;
struct AVFrame;
struct SwsContext;

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

    AVFormatContext* fmt_ctx = nullptr;
    AVStream* stream = nullptr;
    AVCodecContext* codec_ctx = nullptr;
    AVFrame* frame = nullptr;
    SwsContext* sws_ctx = nullptr;

    std::string generateFilename() const;
    void cleanup();
};

#endif
