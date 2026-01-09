#include "MacOSAdapter.hpp"
#include "P2Pro.hpp" // For dprintf
#include <IOKit/IOCFPlugIn.h>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <cstdio>
#include <iostream>

MacOSAdapter::MacOSAdapter() {}

MacOSAdapter::~MacOSAdapter() {
    disconnect();
}

bool MacOSAdapter::connect(uint16_t vid, uint16_t pid) {
    dprintf("MacOSAdapter::connect() - Searching for device VID: 0x%04X, PID: 0x%04X (IOKit)\n", vid, pid);
    
    CFMutableDictionaryRef matchingDict = IOServiceMatching(kIOUSBDeviceClassName);
    if (!matchingDict) {
        dprintf("MacOSAdapter::connect() - Failed to create matching dictionary.\n");
        return false;
    }

    SInt32 vidInt = vid;
    SInt32 pidInt = pid;
    CFNumberRef vidNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &vidInt);
    CFNumberRef pidNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &pidInt);
    CFDictionarySetValue(matchingDict, CFSTR(kUSBVendorID), vidNum);
    CFDictionarySetValue(matchingDict, CFSTR(kUSBProductID), pidNum);
    CFRelease(vidNum);
    CFRelease(pidNum);

    io_iterator_t iter;
    // kIOMasterPortDefault is deprecated but widely used. NULL is also acceptable.
    kern_return_t kr = IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDict, &iter);
    if (kr != KERN_SUCCESS) {
        dprintf("MacOSAdapter::connect() - Failed to get matching services.\n");
        return false;
    }

    io_service_t device;
    bool found = false;
    while ((device = IOIteratorNext(iter))) {
        // Get device name for debugging
        io_name_t deviceName;
        IORegistryEntryGetName(device, deviceName);
        dprintf("MacOSAdapter::connect() - Found device name: %s\n", deviceName);

        IOCFPlugInInterface **plugInInterface = NULL;
        SInt32 score;
        kr = IOCreatePlugInInterfaceForService(device, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &plugInInterface, &score);
        IOObjectRelease(device);
        if (kr != KERN_SUCCESS || !plugInInterface) continue;

        kr = (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID), (LPVOID *)&device_interface);
        (*plugInInterface)->Release(plugInInterface);
        if (kr != KERN_SUCCESS || !device_interface) continue;

        kr = (*device_interface)->USBDeviceOpen(device_interface);
        if (kr == KERN_SUCCESS) {
            is_usb_open = true;
            dprintf("MacOSAdapter::connect() - Device opened successfully via IOKit.\n");
        } else if (kr == kIOReturnExclusiveAccess) {
            dprintf("MacOSAdapter::connect() - Device busy (Exclusive Access), proceeding anyway.\n");
        } else {
            dprintf("MacOSAdapter::connect() - Failed to open device: 0x%08x\n", kr);
        }

        found = true;
        break;
    }
    IOObjectRelease(iter);

    return found;
}

void MacOSAdapter::disconnect() {
    if (cap.isOpened()) {
        cap.release();
    }
    if (ffmpeg_pipe) {
        pclose(ffmpeg_pipe);
        ffmpeg_pipe = nullptr;
        use_ffmpeg_fallback = false;
    }
    if (device_interface) {
        if (is_usb_open) {
            (*device_interface)->USBDeviceClose(device_interface);
            is_usb_open = false;
        }
        (*device_interface)->Release(device_interface);
        device_interface = nullptr;
    }
}

bool MacOSAdapter::control_transfer(uint8_t request_type, uint8_t request, uint16_t value, uint16_t index, 
                                 uint8_t* data, uint16_t length, unsigned int timeout_ms) {
    if (!device_interface) return false;

    IOUSBDevRequestTO req;
    memset(&req, 0, sizeof(req));
    req.bmRequestType = request_type;
    req.bRequest = request;
    req.wValue = value;
    req.wIndex = index;
    req.wLength = length;
    req.pData = data;
    req.completionTimeout = timeout_ms;
    req.noDataTimeout = timeout_ms;

    kern_return_t kr = (*device_interface)->DeviceRequestTO(device_interface, &req);
    if (kr != KERN_SUCCESS) {
        // dprintf("MacOSAdapter::control_transfer() - DeviceRequest failed: 0x%08x\n", kr);
        return false;
    }
    return true;
}

bool MacOSAdapter::is_connected() const {
    return device_interface != nullptr;
}

bool MacOSAdapter::open_video() {
    dprintf("MacOSAdapter::open_video() - Searching for P2Pro Video Stream...\n");
    
    // 1. Try to find the index via ffmpeg list_devices
    int target_index = -1;
    FILE* fp = popen("ffmpeg -f avfoundation -list_devices true -i \"\" 2>&1", "r");
    if (fp) {
        char buf[1024];
        while (fgets(buf, sizeof(buf), fp)) {
            // Looking for lines like: [AVFoundation @ ...] [1] USB-Kamera
            if (strstr(buf, "USB-Kamera") || strstr(buf, "P2 Pro") || strstr(buf, "UVC Camera")) {
                char* p = strchr(buf, '[');
                if (p) {
                    p = strchr(p + 1, '['); // Second '[' should be the index
                    if (p) {
                        target_index = atoi(p + 1);
                        dprintf("MacOSAdapter::open_video() - Found camera by name via ffmpeg at index %d\n", target_index);
                        break;
                    }
                }
            }
        }
        pclose(fp);
    }

    std::vector<int> indices_to_try;
    if (target_index != -1) {
        indices_to_try.push_back(target_index);
    }
    // Add common indices as fallback
    for (int i : {1, 0, 2, 3}) {
        if (i != target_index) indices_to_try.push_back(i);
    }

    for (int i : indices_to_try) {
        dprintf("MacOSAdapter::open_video() - Probing index %d...\n", i);
        
        // Try opening with OpenCV first (simplified strategies)
        try {
            // Strategy 1: Plain open
            cap.open(i, cv::CAP_AVFOUNDATION);
            if (cap.isOpened()) {
                int w = (int)cap.get(cv::CAP_PROP_FRAME_WIDTH);
                int h = (int)cap.get(cv::CAP_PROP_FRAME_HEIGHT);
                if (w == 256 && h == 384) {
                    dprintf("MacOSAdapter::open_video() - OpenCV matched P2Pro on index %d (%dx%d)\n", i, w, h);
                    cap.set(cv::CAP_PROP_CONVERT_RGB, 0);
                    return true;
                }
                cap.release();
            }
        } catch (...) {}

        // Strategy 2: FFmpeg pipe (more robust for this camera on macOS)
        std::string cmd = "ffmpeg -f avfoundation -framerate 25 -video_size 256x384 -i \"" + std::to_string(i) + "\" -f rawvideo -pix_fmt yuyv422 -loglevel quiet -";
        ffmpeg_pipe = popen(cmd.c_str(), "r");
        if (ffmpeg_pipe) {
            // Read one full frame to verify
            size_t frame_size = 256 * 384 * 2;
            std::vector<uint8_t> dummy(frame_size);
            size_t n = fread(dummy.data(), 1, frame_size, ffmpeg_pipe);
            if (n == frame_size) {
                dprintf("MacOSAdapter::open_video() - ffmpeg pipe matched P2Pro on index %d\n", i);
                use_ffmpeg_fallback = true;
                return true;
            }
            pclose(ffmpeg_pipe);
            ffmpeg_pipe = nullptr;
        }
    }

    return false;
}

bool MacOSAdapter::read_frame(std::vector<uint8_t>& frame_data) {
    if (use_ffmpeg_fallback && ffmpeg_pipe) {
        size_t frame_size = 256 * 384 * 2;
        frame_data.resize(frame_size);
        size_t n = fread(frame_data.data(), 1, frame_size, ffmpeg_pipe);
        return (n == frame_size);
    }

    if (!cap.isOpened()) return false;
    cv::Mat frame;
    if (!cap.read(frame)) return false;

    if (frame.empty()) return false;

    size_t size = frame.total() * frame.elemSize();
    frame_data.assign(frame.data, frame.data + size);
    return true;
}
