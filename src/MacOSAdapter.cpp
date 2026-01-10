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
    if (device_interface) return true;
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
    native_cap.close();
    use_native_cap = false;
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
    if (use_native_cap || cap.isOpened()) return true;
    dprintf("MacOSAdapter::open_video() - Searching for P2Pro Video Stream...\n");
    
    // 1. Try Native AVFoundation first
    std::vector<std::string> devices = AVFoundationVideoSource::listDevices();
    std::string target_name = "";
    for (const auto& name : devices) {
        if (name.find("USB-Kamera") != std::string::npos || 
            name.find("P2 Pro") != std::string::npos || 
            name.find("UVC Camera") != std::string::npos) {
            target_name = name;
            break;
        }
    }

    if (!target_name.empty()) {
        dprintf("MacOSAdapter::open_video() - Found camera by name: %s. Attempting native open...\n", target_name.c_str());
        if (native_cap.openByName(target_name, 256, 384, 25)) {
            dprintf("MacOSAdapter::open_video() - Native AVFoundation matched P2Pro\n");
            use_native_cap = true;
            return true;
        }
    }

    // 2. Fallback to index-based native search
    for (int i = 0; i < (int)devices.size(); ++i) {
        dprintf("MacOSAdapter::open_video() - Probing native index %d (%s)...\n", i, devices[i].c_str());
        if (native_cap.open(i, 256, 384, 25)) {
            // Check if we can get a frame (verification)
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            std::vector<uint8_t> dummy;
            if (native_cap.getFrame(dummy)) {
                dprintf("MacOSAdapter::open_video() - Native AVFoundation matched P2Pro on index %d\n", i);
                use_native_cap = true;
                return true;
            }
            native_cap.close();
        }
    }

    // 3. Fallback to OpenCV index-based search (less robust, but native-ish)
    for (int i : {1, 0, 2, 3}) {
        dprintf("MacOSAdapter::open_video() - Probing OpenCV index %d...\n", i);
        try {
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
    }

    return false;
}

bool MacOSAdapter::read_frame(std::vector<uint8_t>& frame_data) {
    if (use_native_cap) {
        return native_cap.getFrame(frame_data);
    }

    if (!cap.isOpened()) return false;
    cv::Mat frame;
    if (!cap.read(frame)) return false;

    if (frame.empty()) return false;

    size_t size = frame.total() * frame.elemSize();
    frame_data.assign(frame.data, frame.data + size);
    return true;
}
