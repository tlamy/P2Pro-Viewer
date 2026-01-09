#include "P2Pro.hpp"
#include "CameraWindow.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <opencv2/opencv.hpp>
#include <vector>

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
        dprintf("Searching for P2Pro camera...\n");
        
        bool cameraConnected = camera.connect();
        if (cameraConnected) {
            dprintf("Connected to P2Pro camera!\n");
            
            auto pn = camera.get_device_info(DeviceInfoType::DEV_INFO_GET_PN);
            dprintf("Part Number: ");
            for (auto b : pn) {
                if (b >= 32 && b <= 126) dprintf("%c", (char)b);
                else dprintf("[%02X]", b);
            }
            dprintf("\n");

            camera.pseudo_color_set(0, PseudoColorTypes::PSEUDO_IRON_RED);
        } else {
            dprintf("Could not find or connect to P2Pro camera via libusb. Commands will not work.\n");
        }

        // OpenCV Video Capture
        dprintf("Searching for P2Pro Video Stream via OpenCV...\n");
        cv::VideoCapture cap;
        bool capOpened = false;

        // Try to find the camera by resolution (256x384 @ 25fps)
        // On macOS/Linux/Windows this might vary, but let's try a few indices
        for (int i = 0; i < 10; ++i) {
            dprintf("Trying OpenCV index %d...\n", i);
            cap.open(i);
            if (cap.isOpened()) {
                int w = (int)cap.get(cv::CAP_PROP_FRAME_WIDTH);
                int h = (int)cap.get(cv::CAP_PROP_FRAME_HEIGHT);
                double fps = cap.get(cv::CAP_PROP_FPS);
                dprintf("Index %d opened: %dx%d @ %.1f FPS\n", i, w, h, fps);
                if (w == 256 && h == 384) {
                    dprintf("Found P2Pro Video Stream on index %d\n", i);
                    capOpened = true;
                    break;
                }
                cap.release();
            } else {
                dprintf("Index %d could not be opened.\n", i);
            }
        }

        if (!capOpened) {
            dprintf("Could not find P2Pro Video Stream.\n");
        } else {
            dprintf("Setting CAP_PROP_CONVERT_RGB to 0...\n");
            cap.set(cv::CAP_PROP_CONVERT_RGB, 0); // Disable automatic conversion if possible
        }

        dprintf("Entering main loop...\n");
        bool running = true;
        cv::Mat frame;
        while (running) {
            window.pollEvents(running);
            
            if (capOpened && cap.read(frame)) {
                // dprintf("Frame read: %dx%d\n", frame.cols, frame.rows);
                // frame is 256x384 YUYV (effectively 256x384 uint8_t in some backends, 
                // or 256x384x2 if YUYV is treated as 2 bytes per pixel)
                // In Python: picture_data = frame[0:frame_mid_pos], thermal_data = frame[frame_mid_pos:]
                
                // If OpenCV returns YUYV as a 384x256 image with 2 channels:
                if (frame.rows == 384 && frame.cols == 256) {
                    cv::Mat pseudo_yuyv = frame(cv::Rect(0, 0, 256, 192));
                    cv::Mat pseudo_rgb;
                    cv::cvtColor(pseudo_yuyv, pseudo_rgb, cv::COLOR_YUV2RGB_YUY2);
                    
                    std::vector<uint8_t> rgb_vec;
                    if (pseudo_rgb.isContinuous()) {
                        rgb_vec.assign(pseudo_rgb.data, pseudo_rgb.data + pseudo_rgb.total() * pseudo_rgb.channels());
                    } else {
                        for (int i = 0; i < pseudo_rgb.rows; ++i) {
                            rgb_vec.insert(rgb_vec.end(), pseudo_rgb.ptr<uint8_t>(i), pseudo_rgb.ptr<uint8_t>(i) + pseudo_rgb.cols * 3);
                        }
                    }
                    window.updateFrame(rgb_vec, 256, 192);
                }
            }

            window.render();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

    } catch (const std::exception& e) {
        dprintf("Error: %s\n", e.what());
        return -1;
    }

    return 0;
}
