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
        dprintf("Connecting to P2Pro camera (USB and Video)...\n");
        
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
            dprintf("Could not find or connect to P2Pro camera. Please ensure it is plugged in.\n");
            return -1;
        }

        dprintf("Entering main loop...\n");
        bool running = true;
        while (running) {
            window.pollEvents(running);
            
            P2ProFrame frame;
            if (cameraConnected && camera.get_frame(frame)) {
                window.updateFrame(frame.rgb, 256, 192);
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
