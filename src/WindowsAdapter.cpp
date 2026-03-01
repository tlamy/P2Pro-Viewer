#include "WindowsAdapter.hpp"
#include "P2Pro.hpp" // For dprintf
#include <iostream>
#include <thread>
#include <chrono>

WindowsAdapter::WindowsAdapter() {
    if (libusb_init(&ctx) < 0) {
        dprintf("WindowsAdapter - Failed to initialize libusb\n");
    }

#if defined(_WIN32) && defined(LIBUSB_API_VERSION) && (LIBUSB_API_VERSION >= 0x01000106)
    // On Windows, the default backend is often WinUSB, but P2Pro may work better 
    // with libusb-win32 filter which is handled well by libusb.
    // Sometimes forcing specific options can help if multiple backends are available.
#endif
}

WindowsAdapter::~WindowsAdapter() {
    disconnect();
    if (ctx) {
        libusb_exit(ctx);
    }
}

bool WindowsAdapter::connect(uint16_t vid, uint16_t pid) {
    if (!ctx) return false;
    if (dev_handle) return true;

    dprintf("WindowsAdapter::connect() - Searching for device VID: 0x%04X, PID: 0x%04X\n", vid, pid);

    // Some devices require a small pause after video open before USB control transfer starts.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Try to list all devices to find the correct one and diagnose issues
    libusb_device **devs;
    ssize_t cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0) {
        dprintf("WindowsAdapter::connect() - Failed to get device list (%d)\n", (int)cnt);
        return false;
    }

    libusb_device *found = nullptr;
    for (ssize_t i = 0; i < cnt; i++) {
        struct libusb_device_descriptor desc;
        int res = libusb_get_device_descriptor(devs[i], &desc);
        if (res < 0) continue;

        if (desc.idVendor == vid && desc.idProduct == pid) {
            dprintf("WindowsAdapter::connect() - Found device at bus %d, addr %d\n", 
                    libusb_get_bus_number(devs[i]), libusb_get_device_address(devs[i]));
            found = devs[i];
            break;
        }
    }

    if (!found) {
        dprintf("WindowsAdapter::connect() - Device not found in libusb device list.\n");
        dprintf("Check if the device is plugged in and recognized by Windows.\n");
        libusb_free_device_list(devs, 1);
        return false;
    }

    int res = libusb_open(found, &dev_handle);
    if (res < 0) {
        dprintf("WindowsAdapter::connect() - Could not open device for USB control: %s (%d)\n", libusb_error_name(res), res);
        dprintf("  Running in VIDEO-ONLY mode. Camera commands (color palette, etc.) are unavailable.\n");
        dprintf("  Root cause: libusb 1.0 requires WinUSB as the function driver, but the UVC driver\n");
        dprintf("  (usbvideo.sys) is needed for Media Foundation video — both cannot be active at once.\n");
        libusb_free_device_list(devs, 1);
        return true;  // Video works; USB control is optional
    }

    libusb_free_device_list(devs, 1);

    // Some devices require a reset or configuration to be set, but for P2Pro control transfers usually work directly.
    dprintf("WindowsAdapter::connect() - Device opened successfully.\n");
    return true;
}

void WindowsAdapter::disconnect() {
    mf_cap.close();
    if (dev_handle) {
        libusb_close(dev_handle);
        dev_handle = nullptr;
    }
}

bool WindowsAdapter::control_transfer(uint8_t request_type, uint8_t request, uint16_t value, uint16_t index,
                                      uint8_t *data, uint16_t length, unsigned int timeout_ms) {
    if (!dev_handle) return false;

    // dprintf("WindowsAdapter::control_transfer(rt:0x%02X, r:0x%02X, v:0x%04X, i:0x%04X, len:%d)\n", 
    //        request_type, request, value, index, length);

    int res = libusb_control_transfer(dev_handle, request_type, request, value, index, data, length, timeout_ms);
    if (res < 0) {
        // dprintf("WindowsAdapter::control_transfer - libusb error: %s (%d)\n", libusb_error_name(res), res);
        return false;
    }
    return true;
}

bool WindowsAdapter::is_connected() const {
    return dev_handle != nullptr;
}

bool WindowsAdapter::open_video() {
    if (mf_cap.isOpened()) return true;
    dprintf("WindowsAdapter::open_video() - Searching for P2Pro Video Stream via Media Foundation...\n");

    // Try to open by name "USB Camera" which is common for P2Pro
    // Or just try the first available camera
    if (mf_cap.open(256, 384)) {
        dprintf("WindowsAdapter::open_video() - Media Foundation matched P2Pro\n");
        return true;
    }

    return false;
}

bool WindowsAdapter::read_frame(std::vector<uint8_t> &frame_data) {
    return mf_cap.getFrame(frame_data);
}
