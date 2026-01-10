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
    if (cap.isOpened()) {
        cap.release();
    }
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
    if (cap.isOpened()) return true;
    dprintf("LinuxAdapter::open_video() - Searching for P2Pro Video Stream...\n");

    // On Linux, P2Pro usually shows up as /dev/videoX.
    // We can try multiple indices.
    for (int i: {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10}) {
        dprintf("LinuxAdapter::open_video() - Probing index %d...\n", i);
        try {
            // Try V4L2 first as it's more direct on Linux
            cap.open(i, cv::CAP_V4L2);
            if (!cap.isOpened()) {
                // Try any backend as fallback
                cap.open(i, cv::CAP_ANY);
            }

            if (cap.isOpened()) {
                int w = (int) cap.get(cv::CAP_PROP_FRAME_WIDTH);
                int h = (int) cap.get(cv::CAP_PROP_FRAME_HEIGHT);
                dprintf("LinuxAdapter::open_video() - Index %d: %dx%d\n", i, w, h);
                if (w == 256 && h == 384) {
                    dprintf("LinuxAdapter::open_video() - OpenCV matched P2Pro on index %d (%dx%d)\n", i, w, h);
                    cap.set(cv::CAP_PROP_CONVERT_RGB, 0);
                    return true;
                }
                cap.release();
            }
        } catch (...) {
        }
    }

    return false;
}

bool LinuxAdapter::read_frame(std::vector<uint8_t> &frame_data) {
    if (!cap.isOpened()) return false;
    cv::Mat frame;
    if (!cap.read(frame)) return false;

    if (frame.empty()) return false;

    size_t size = frame.total() * frame.elemSize();
    frame_data.assign(frame.data, frame.data + size);
    return true;
}
