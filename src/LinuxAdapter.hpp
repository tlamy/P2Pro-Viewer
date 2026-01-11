#ifndef LINUX_ADAPTER_HPP
#define LINUX_ADAPTER_HPP

#include "USBAdapter.hpp"
#include "V4L2VideoSource.hpp"
#include <libusb-1.0/libusb.h>

class LinuxAdapter : public USBAdapter {
public:
    LinuxAdapter();

    virtual ~LinuxAdapter();

    bool connect(uint16_t vid, uint16_t pid) override;

    void disconnect() override;

    bool control_transfer(uint8_t request_type, uint8_t request, uint16_t value, uint16_t index,
                          uint8_t *data, uint16_t length, unsigned int timeout_ms) override;

    bool is_connected() const override;

    bool open_video() override;

    bool read_frame(std::vector<uint8_t> &frame_data) override;

private:
    libusb_context *ctx = nullptr;
    libusb_device_handle *dev_handle = nullptr;
    V4L2VideoSource v4l2_cap;
};

#endif