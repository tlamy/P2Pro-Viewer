#include "P2Pro.hpp"
#include "CameraWindow.hpp"
#include "VideoRecorder.hpp"
#include "ColorConversion.hpp"
#include "Preferences.hpp"
#include "HotSpotTracker.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <deque>
#include <cmath>

HotSpotResult detectHotSpot(const P2ProFrame &frame, bool previouslyFound) {
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

    if ((double) maxVal - avgVal > threshold) {
        res.val = maxVal;
        res.tempC = (maxVal / 64.0) - 273.15;
    } else {
        res.found = false;
    }
    return res;
}

void annotateFrame(uint8_t* rgb, int width, int height, const HotSpotResult &res, TTF_Font* font) {
    if (!res.found) return;

    // Use high-contrast colors: White with Black shadow
    SDL_Color mainColor = { 255, 255, 255, 255 };
    SDL_Color shadowColor = { 0, 0, 0, 255 };

    // Simple crosshair drawing (White with Black shadow/outline)
    int crossSize = 10;
    auto drawPixel = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b) {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            int idx = (y * width + x) * 3;
            rgb[idx] = r;
            rgb[idx + 1] = g;
            rgb[idx + 2] = b;
        }
    };

    // Draw shadow first (1px offset)
    for (int i = -crossSize; i <= crossSize; ++i) {
        drawPixel(res.x + i + 1, res.y + 1, 0, 0, 0);
        drawPixel(res.x + 1, res.y + i + 1, 0, 0, 0);
    }
    // Draw crosshair
    for (int i = -crossSize; i <= crossSize; ++i) {
        drawPixel(res.x + i, res.y, 255, 255, 255);
        drawPixel(res.x, res.y + i, 255, 255, 255);
    }

    // Draw temperature text
    char text[32];
    snprintf(text, sizeof(text), "%.1f C", res.tempC);
    
    // Position text near crosshair, ensuring it stays within bounds
    int tx = res.x + 12;
    int ty = res.y - 12;
    if (tx + 60 > width) tx = res.x - 65;
    if (ty < 15) ty = res.y + 20;

    if (font) {
        ColorConversion::drawTTFText(rgb, width, height, tx, ty, text, font, mainColor, shadowColor);
    }
}

int main(int argc, char *argv[]) {
    try {
        Preferences& prefs = Preferences::getInstance();
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
            for (auto b: pn) {
                if (b >= 32 && b <= 126) dprintf("%c", (char) b);
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
        std::vector<uint8_t> recordingBuffer;
        std::vector<uint8_t> rotationBuffer;
        int recWidth = 0, recHeight = 0;
        float recScale = 1.0f;
        int recRotation = 0;

        while (running) {
            window.pollEvents(running, recordToggleRequested, recorder.isRecording());

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
                    recordingBuffer.clear();
                    rotationBuffer.clear();
                } else {
                    recScale = window.getScale();
                    recRotation = window.getRotation();
                    
                    int baseW = 256;
                    int baseH = 192;
                    if (recRotation == 90 || recRotation == 270) {
                        std::swap(baseW, baseH);
                    }

                    recWidth = (int)(baseW * recScale);
                    recHeight = (int)(baseH * recScale);
                    recordingBuffer.resize(recWidth * recHeight * 3);
                    rotationBuffer.resize(256 * 192 * 3);
                    // Start recording (recWidth x recHeight at 25 fps)
                    recorder.start(recWidth, recHeight, 25.0);
                }
            }

            static float smoothedMin = 16384.0f;
            static float smoothedMax = 0.0f;
            static std::vector<uint8_t> customRgb;
            static std::vector<uint8_t> devRgb;
            customRgb.resize(256 * 192 * 3);

            P2ProFrame frame;
            static int readFailCount = 0;
            if (cameraConnected) {
                if (!camera.is_connected()) {
                    dprintf("Camera connection lost (USB/Video)!\n");
                    cameraConnected = false;
                    camera.disconnect();
                    if (recorder.isRecording()) {
                        dprintf("Stopping recording due to connection loss.\n");
                        recorder.stop();
                    }
                    hs.found = false;
                }
            }

            if (cameraConnected) {
                if (camera.get_frame(frame)) {
                    readFailCount = 0;

                    // Calculate min/max of thermal data
                    uint16_t currentMin = 65535, currentMax = 0;
                    for (uint16_t v : frame.thermal) {
                        if (v < currentMin) currentMin = v;
                        if (v > currentMax) currentMax = v;
                    }

                    // Smooth range over time
                    float alpha = 0.1f; // Smoothing factor
                    smoothedMin = smoothedMin * (1.0f - alpha) + (float)currentMin * alpha;
                    smoothedMax = smoothedMax * (1.0f - alpha) + (float)currentMax * alpha;

                    // Generate custom color image
                    ColorConversion::applyPalette(frame.thermal.data(), customRgb.data(), 256, 192, 
                                                 (uint16_t)smoothedMin, (uint16_t)smoothedMax, 
                                                 window.getPalette(), window.getGamma());

                    hs = detectHotSpot(frame, hs.found);
                    tracker.update(hs, frame);

                    // Update window
                    if (window.isDevMode()) {
                        devRgb.resize(512 * 192 * 3);
                        for (int y = 0; y < 192; ++y) {
                            memcpy(devRgb.data() + (y * 512 + 0) * 3, frame.rgb.data() + (y * 256) * 3, 256 * 3);
                            memcpy(devRgb.data() + (y * 512 + 256) * 3, customRgb.data() + (y * 256) * 3, 256 * 3);
                        }
                        window.updateFrame(devRgb, frame.thermal, 256, 192);
                    } else {
                        window.updateFrame(customRgb, frame.thermal, 256, 192);
                    }

                    if (recorder.isRecording()) {
                        const uint8_t* srcRgb = customRgb.data();
                        int srcW = 256;
                        int srcH = 192;

                        if (recRotation != 0) {
                            ColorConversion::rotateRGB24(customRgb.data(), 256, 192, rotationBuffer.data(), recRotation);
                            srcRgb = rotationBuffer.data();
                            if (recRotation == 90 || recRotation == 270) {
                                std::swap(srcW, srcH);
                            }
                        }

                        ColorConversion::scaleRGB24(srcRgb, srcW, srcH, recordingBuffer.data(), recWidth, recHeight);
                        
                        HotSpotResult scaledHs = hs;
                        // Transform hs coordinates based on rotation
                        if (recRotation == 90) {
                            int nx = hs.y;
                            int ny = 256 - 1 - hs.x;
                            scaledHs.x = nx;
                            scaledHs.y = ny;
                        } else if (recRotation == 180) {
                            scaledHs.x = 256 - 1 - hs.x;
                            scaledHs.y = 192 - 1 - hs.y;
                        } else if (recRotation == 270) {
                            int nx = 192 - 1 - hs.y;
                            int ny = hs.x;
                            scaledHs.x = nx;
                            scaledHs.y = ny;
                        }

                        scaledHs.x = (int)(scaledHs.x * recScale);
                        scaledHs.y = (int)(scaledHs.y * recScale);
                        
                        annotateFrame(recordingBuffer.data(), recWidth, recHeight, scaledHs, window.getRecordingFont());
                        recorder.writeFrame(recordingBuffer.data());
                    }
                } else {
                    readFailCount++;
                    if (readFailCount > 50) { // 50 attempts @ ~10ms = ~500ms
                        dprintf("Camera disconnected!\n");
                        cameraConnected = false;
                        camera.disconnect();
                        if (recorder.isRecording()) {
                            dprintf("Stopping recording due to disconnection.\n");
                            recorder.stop();
                        }
                        hs.found = false;
                        readFailCount = 0;
                    }
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
    } catch (const std::exception &e) {
        dprintf("Error: %s\n", e.what());
        return -1;
    }

    return 0;
}