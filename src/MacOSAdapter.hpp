#ifndef MACOS_ADAPTER_HPP
#define MACOS_ADAPTER_HPP

#include "USBAdapter.hpp"
#include "AVFoundationVideoSource.hpp"
#include <IOKit/usb/IOUSBLib.h>
#include <CoreFoundation/CoreFoundation.h>

class MacOSAdapter : public USBAdapter {
public:
    MacOSAdapter();
    virtual ~MacOSAdapter();

    bool connect(uint16_t vid, uint16_t pid) override;
    void disconnect() override;

    bool control_transfer(uint8_t request_type, uint8_t request, uint16_t value, uint16_t index, 
                         uint8_t* data, uint16_t length, unsigned int timeout_ms) override;

    bool is_connected() const override;

    bool open_video() override;
    bool read_frame(std::vector<uint8_t>& frame_data) override;

private:
    IOUSBDeviceInterface **device_interface = nullptr;
    bool is_usb_open = false;
    AVFoundationVideoSource native_cap;
};

#endif
