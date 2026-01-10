#include "VideoRecorder.hpp"
#include "P2Pro.hpp" // For dprintf
#include <ctime>
#include <iomanip>
#include <sstream>

VideoRecorder::VideoRecorder() {}

VideoRecorder::~VideoRecorder() {
    stop();
}

std::string VideoRecorder::generateFilename() const {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S") << ".mkv";
    return oss.str();
}

bool VideoRecorder::start(int w, int h, double fps) {
    if (recording) return false;

    filename = generateFilename();
    width = w;
    height = h;

    // Try MKV format with various codecs
    // X264 is common for MKV
    int fourcc = cv::VideoWriter::fourcc('X', '2', '6', '4');
    writer.open(filename, fourcc, fps, cv::Size(width, height), true);

    if (!writer.isOpened()) {
        dprintf("VideoRecorder::start() - Failed to open VideoWriter with X264, trying mp4v...\n");
        fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
        writer.open(filename, fourcc, fps, cv::Size(width, height), true);
    }

    if (!writer.isOpened()) {
        dprintf("VideoRecorder::start() - Failed to open VideoWriter with mp4v, trying avc1 and .mp4...\n");
        filename = filename.substr(0, filename.length() - 4) + ".mp4";
        fourcc = cv::VideoWriter::fourcc('a', 'v', 'c', '1');
        writer.open(filename, fourcc, fps, cv::Size(width, height), true);
    }

    if (!writer.isOpened()) {
        dprintf("VideoRecorder::start() - Failed to open VideoWriter with avc1, trying MJPG and .avi...\n");
        filename = filename.substr(0, filename.length() - 4) + ".avi";
        fourcc = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
        writer.open(filename, fourcc, fps, cv::Size(width, height), true);
    }

    if (!writer.isOpened()) {
        dprintf("VideoRecorder::start() - Failed to open VideoWriter at all!\n");
        return false;
    }

    dprintf("VideoRecorder::start() - Recording started: %s (%dx%d @ %.1f FPS)\n", filename.c_str(), width, height, fps);
    recording = true;
    return true;
}

void VideoRecorder::stop() {
    if (!recording) return;
    writer.release();
    recording = false;
    dprintf("VideoRecorder::stop() - Recording stopped: %s\n", filename.c_str());
}

void VideoRecorder::writeFrame(const std::vector<uint8_t>& rgb_data) {
    if (!recording) return;
    if (rgb_data.size() != static_cast<size_t>(width * height * 3)) {
        return;
    }

    // OpenCV VideoWriter expects BGR by default
    cv::Mat frame_rgb(height, width, CV_8UC3, const_cast<uint8_t*>(rgb_data.data()));
    cv::Mat frame_bgr;
    cv::cvtColor(frame_rgb, frame_bgr, cv::COLOR_RGB2BGR);
    writer.write(frame_bgr);
}
