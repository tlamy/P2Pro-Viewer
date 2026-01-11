#include "LinuxAdapter.hpp"
#include "P2Pro.hpp" // For dprintf
#include <iostream>
#include <thread>
#include <chrono>

LinuxAdapter::LinuxAdapter() {
    if (libusb_init(&ctx) < 0) {
        dprintf("LinuxAdapter - Failed to initialize libusb\n");
    }
}

LinuxAdapter::~LinuxAdapter() {
    disconnect();
    if (ctx) {
        libusb_exit(ctx);
    }
}

bool LinuxAdapter::connect(uint16_t vid, uint16_t pid) {
    if (!ctx) return false;
    if (dev_handle) return true;

    dprintf("LinuxAdapter::connect() - Searching for device VID: 0x%04X, PID: 0x%04X\n", vid, pid);
    dev_handle = libusb_open_device_with_vid_pid(ctx, vid, pid);
    if (!dev_handle) {
        dprintf(
            "LinuxAdapter::connect() - Device not found or permission denied. Checking if we need to detach kernel driver...\n");
        // If it failed, it might be because the device is not found OR it's already in use.
        // But libusb_open_device_with_vid_pid should at least return a handle if it can open it.
        return false;
    }

    // We don't detach the kernel driver or claim the interface here.
    // Detaching the kernel driver would make the V4L2 device (/dev/videoX) disappear.
    // Most control transfers for P2Pro work even if the kernel driver is attached,
    // as long as we have permissions to the USB device node.

    dprintf("LinuxAdapter::connect() - Device opened successfully.\n");

    return true;
}

void LinuxAdapter::disconnect() {
    v4l2_cap.close();
    if (dev_handle) {
        libusb_close(dev_handle);
        dev_handle = nullptr;
    }
}

bool LinuxAdapter::control_transfer(uint8_t request_type, uint8_t request, uint16_t value, uint16_t index,
                                    uint8_t *data, uint16_t length, unsigned int timeout_ms) {
    if (!dev_handle) return false;

    int res = libusb_control_transfer(dev_handle, request_type, request, value, index, data, length, timeout_ms);
    if (res < 0) {
        // dprintf("LinuxAdapter::control_transfer() - failed: %s\n", libusb_error_name(res));
        return false;
    }
    return true;
}

bool LinuxAdapter::is_connected() const {
    return dev_handle != nullptr;
}

bool LinuxAdapter::open_video() {
    if (v4l2_cap.isOpened()) return true;
    dprintf("LinuxAdapter::open_video() - Searching for P2Pro Video Stream...\n");

    // On Linux, P2Pro usually shows up as /dev/videoX.
    // We can try multiple indices.
    for (int i: {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10}) {
        std::string device = "/dev/video" + std::to_string(i);
        dprintf("LinuxAdapter::open_video() - Probing %s...\n", device.c_str());
        
        if (v4l2_cap.open(device, 256, 384)) {
            // Check if we can get a frame (verification)
            // We give it a bit more time and multiple attempts to get the first frame
            bool got_frame = false;
            for (int attempt = 0; attempt < 10; ++attempt) {
                std::vector<uint8_t> dummy;
                if (v4l2_cap.getFrame(dummy)) {
                    if (dummy.size() >= 256 * 384 * 2) {
                        got_frame = true;
                        break;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            if (got_frame) {
                dprintf("LinuxAdapter::open_video() - V4L2 matched P2Pro on %s\n", device.c_str());
                return true;
            }
            v4l2_cap.close();
        }
    }

    return false;
}

bool LinuxAdapter::read_frame(std::vector<uint8_t> &frame_data) {
    return v4l2_cap.getFrame(frame_data);
}
