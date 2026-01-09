#include "MacOSAdapter.hpp"
#include "P2Pro.hpp" // For dprintf
#include <IOKit/IOCFPlugIn.h>
#include <stdexcept>

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
        IOCFPlugInInterface **plugInInterface = NULL;
        SInt32 score;
        kr = IOCreatePlugInInterfaceForService(device, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &plugInInterface, &score);
        IOObjectRelease(device);
        if (kr != KERN_SUCCESS || !plugInInterface) continue;

        kr = (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID), (LPVOID *)&device_interface);
        (*plugInInterface)->Release(plugInInterface);
        if (kr != KERN_SUCCESS || !device_interface) continue;

        // Try to open the device. 
        kr = (*device_interface)->USBDeviceOpen(device_interface);
        // We avoid USBDeviceOpenSeize(device_interface) because it might detach the UVC driver,
        // which we need for the OpenCV video stream.

        if (kr == KERN_SUCCESS) {
            dprintf("MacOSAdapter::connect() - Device opened successfully via IOKit.\n");
            found = true;
            break;
        } else {
            dprintf("MacOSAdapter::connect() - Failed to open device: 0x%08x\n", kr);
            (*device_interface)->Release(device_interface);
            device_interface = nullptr;
        }
    }
    IOObjectRelease(iter);

    return found;
}

void MacOSAdapter::disconnect() {
    if (device_interface) {
        (*device_interface)->USBDeviceClose(device_interface);
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
