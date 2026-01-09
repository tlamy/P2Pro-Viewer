#ifndef USB_ADAPTER_HPP
#define USB_ADAPTER_HPP

#include <vector>
#include <cstdint>
#include <string>

class USBAdapter {
public:
    virtual ~USBAdapter() = default;

    virtual bool connect(uint16_t vid, uint16_t pid) = 0;
    virtual void disconnect() = 0;

    virtual bool control_transfer(uint8_t request_type, uint8_t request, uint16_t value, uint16_t index, 
                                 uint8_t* data, uint16_t length, unsigned int timeout_ms) = 0;

    virtual bool is_connected() const = 0;

    virtual bool open_video() = 0;
    virtual bool read_frame(std::vector<uint8_t>& frame_data) = 0;
};

#endif
