#ifndef VIDEO_RECORDER_HPP
#define VIDEO_RECORDER_HPP

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

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
    cv::VideoWriter writer;
    bool recording = false;
    std::string filename;
    int width = 0;
    int height = 0;

    std::string generateFilename() const;
};

#endif
