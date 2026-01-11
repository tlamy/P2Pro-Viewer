#include "P2Pro.hpp"
#include "CameraWindow.hpp"
#include "VideoRecorder.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <deque>
#include <cmath>

class HotSpotTracker {
public:
    struct Sample {
        double x, y, temp;
        uint8_t r, g, b;
    };

    void update(HotSpotResult& res, const P2ProFrame& frame) {
        if (!res.found) {
            lostFrames++;
            // Persistence: if we recently had a hot spot, keep it for up to 3 frames
            if (lostFrames <= 3 && !history.empty()) {
                res.found = true;
                applyHistory(res);
            } else {
                res.found = false;
                if (lostFrames > 10) history.clear();
            }
            return;
        }

        lostFrames = 0;
        Sample current = {(double)res.x, (double)res.y, res.tempC, res.r, res.g, res.b};

        if (!history.empty()) {
            double lastX = history.back().x;
            double lastY = history.back().y;
            double distSq = std::pow(current.x - lastX, 2) + std::pow(current.y - lastY, 2);
            
            if (distSq > 20.0 * 20.0) { // Threshold for "moving significantly"
                history.clear();
            }
        }
        
        history.push_back(current);
        if (history.size() > 8) {
            history.pop_front();
        }

        applyHistory(res);
    }

private:
    std::deque<Sample> history;
    int lostFrames = 0;

    void applyHistory(HotSpotResult& res) {
        if (history.empty()) return;

        double avgX = 0, avgY = 0;
        double maxTemp = -1000.0;
        Sample bestSample = history.back();

        for (const auto& s : history) {
            avgX += s.x;
            avgY += s.y;
            if (s.temp > maxTemp) {
                maxTemp = s.temp;
                bestSample = s; // Use color and temp from the hottest sample
            }
        }
        
        res.x = (int)std::round(avgX / history.size());
        res.y = (int)std::round(avgY / history.size());
        // Use max temperature from buffer as requested: "if any modification is done to temperature, it must use max, not average"
        res.tempC = maxTemp;
        
        // Use color from the hottest sample for better contrast consistency
        res.r = bestSample.r;
        res.g = bestSample.g;
        res.b = bestSample.b;
    }
};

HotSpotResult detectHotSpot(const P2ProFrame& frame, bool previouslyFound) {
    if (frame.thermal.empty()) return {};

    int width = 256;
    int height = 192;
    HotSpotResult res;
    res.found = false;
    uint16_t maxVal = 0;
    double totalSum = 0;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint16_t val = frame.thermal[y * width + x];
            totalSum += val;
            if (!res.found || val > maxVal) {
                res.found = true;
                res.x = x;
                res.y = y;
                maxVal = val;
            }
        }
    }

    if (!res.found) return res;

    double avgVal = totalSum / (width * height);
    double threshold = previouslyFound ? 96.0 : 128.0; // Hysteresis: 1.5C vs 2.0C

    if ((double)maxVal - avgVal > threshold) {
        res.val = maxVal;
        res.tempC = (maxVal / 64.0) - 273.15;
        
        // Extract color from RGB frame
        if (frame.rgb.size() >= (size_t)(res.y * width + res.x) * 3 + 3) {
            int idx = (res.y * width + res.x) * 3;
            res.r = frame.rgb[idx];
            res.g = frame.rgb[idx+1];
            res.b = frame.rgb[idx+2];
        }
    } else {
        res.found = false;
    }
    return res;
}

void annotateFrame(P2ProFrame& frame, const HotSpotResult& res) {
    if (!res.found) return;

    int width = 256;
    int height = 192;
    uint8_t r = 255 - res.r;
    uint8_t g = 255 - res.g;
    uint8_t b = 255 - res.b;

    // Simple crosshair drawing
    int crossSize = 10;
    for (int i = -crossSize; i <= crossSize; ++i) {
        if (res.x + i >= 0 && res.x + i < width) {
            int idx = (res.y * width + (res.x + i)) * 3;
            frame.rgb[idx] = r;
            frame.rgb[idx + 1] = g;
            frame.rgb[idx + 2] = b;
        }
        if (res.y + i >= 0 && res.y + i < height) {
            int idx = ((res.y + i) * width + res.x) * 3;
            frame.rgb[idx] = r;
            frame.rgb[idx + 1] = g;
            frame.rgb[idx + 2] = b;
        }
    }
}

int main(int argc, char* argv[]) {
    try {
        dprintf("Application Start\n");
        CameraWindow window("P2Pro Viewer", 256, 192); // Display only the pseudo color part initially
        dprintf("Initializing Window...\n");
        if (!window.init()) {
            dprintf("Failed to initialize window.\n");
            return -1;
        }

        dprintf("Initializing P2Pro camera object...\n");
        P2Pro camera;
        dprintf("Connecting to P2Pro camera (USB and Video)...\n");
        
        bool cameraConnected = camera.connect();
        if (!cameraConnected) {
            dprintf("Could not find or connect to P2Pro camera. Entering scanning mode...\n");
        } else {
            dprintf("Connected to P2Pro camera!\n");
            
            auto pn = camera.get_device_info(DeviceInfoType::DEV_INFO_GET_PN);
            dprintf("Part Number: ");
            for (auto b : pn) {
                if (b >= 32 && b <= 126) dprintf("%c", (char)b);
                else dprintf("[%02X]", b);
            }
            dprintf("\n");

            camera.pseudo_color_set(0, PseudoColorTypes::PSEUDO_IRON_RED);
        }

        dprintf("Entering main loop...\n");
        bool running = true;
        VideoRecorder recorder;
        HotSpotTracker tracker;
        bool indicatorVisible = true;
        auto lastBlinkTime = std::chrono::steady_clock::now();
        auto lastConnectAttempt = std::chrono::steady_clock::now();
        bool recordToggleRequested = false;
        HotSpotResult hs;

        while (running) {
            window.pollEvents(running, recordToggleRequested);
            
            if (!cameraConnected) {
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::seconds>(now - lastConnectAttempt).count() >= 1) {
                    lastConnectAttempt = now;
                    if (camera.connect()) {
                        dprintf("Reconnected to P2Pro camera!\n");
                        cameraConnected = true;
                        camera.pseudo_color_set(0, PseudoColorTypes::PSEUDO_IRON_RED);
                    }
                }
            }

            if (recordToggleRequested && cameraConnected) {
                if (recorder.isRecording()) {
                    recorder.stop();
                } else {
                    // Start recording (256x192 at 25 fps)
                    recorder.start(256, 192, 25.0);
                }
            }

            P2ProFrame frame;
            if (cameraConnected) {
                if (camera.get_frame(frame)) {
                    hs = detectHotSpot(frame, hs.found);
                    tracker.update(hs, frame);
                    
                    // Update window with clean frame (overlay rendered separately)
                    window.updateFrame(frame.rgb, 256, 192);
                    
                    if (recorder.isRecording()) {
                        P2ProFrame annotated = frame;
                        annotateFrame(annotated, hs);
                        recorder.writeFrame(annotated.rgb);
                    }
                } else {
                    dprintf("Camera disconnected!\n");
                    cameraConnected = false;
                    camera.disconnect();
                    if (recorder.isRecording()) {
                        dprintf("Stopping recording due to disconnection.\n");
                        recorder.stop();
                    }
                    hs.found = false;
                }
            }

            window.render(recorder.isRecording(), indicatorVisible, cameraConnected, hs);

            if (recorder.isRecording()) {
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastBlinkTime).count() > 500) {
                    indicatorVisible = !indicatorVisible;
                    lastBlinkTime = now;
                }
            } else {
                indicatorVisible = false;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        if (recorder.isRecording()) {
            recorder.stop();
        }

    } catch (const std::exception& e) {
        dprintf("Error: %s\n", e.what());
        return -1;
    }

    return 0;
}
