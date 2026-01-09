#ifndef P2PRO_HPP
#define P2PRO_HPP

#include "USBAdapter.hpp"
#include <vector>
#include <string>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <memory>

void dprintf(const char* format, ...);

enum class PseudoColorTypes : uint8_t {
    PSEUDO_WHITE_HOT = 1,
    PSEUDO_IRON_RED = 3,
    PSEUDO_RAINBOW_1 = 4,
    PSEUDO_RAINBOW_2 = 5,
    PSEUDO_RAINBOW_3 = 6,
    PSEUDO_RED_HOT = 7,
    PSEUDO_HOT_RED = 8,
    PSEUDO_RAINBOW_4 = 9,
    PSEUDO_RAINBOW_5 = 10,
    PSEUDO_BLACK_HOT = 11
};

enum class PropTpdParams : uint16_t {
    TPD_PROP_DISTANCE = 0,
    TPD_PROP_TU = 1,
    TPD_PROP_TA = 2,
    TPD_PROP_EMS = 3,
    TPD_PROP_TAU = 4,
    TPD_PROP_GAIN_SEL = 5
};

enum class DeviceInfoType : uint16_t {
    DEV_INFO_CHIP_ID = 0,
    DEV_INFO_FW_COMPILE_DATE = 1,
    DEV_INFO_DEV_QUALIFICATION = 2,
    DEV_INFO_IR_INFO = 3,
    DEV_INFO_PROJECT_INFO = 4,
    DEV_INFO_FW_BUILD_VERSION_INFO = 5,
    DEV_INFO_GET_PN = 6,
    DEV_INFO_GET_SN = 7,
    DEV_INFO_GET_SENSOR_ID = 8
};

struct P2ProFrame {
    std::vector<uint8_t> rgb;      // 256x192x3
    std::vector<uint16_t> thermal; // 256x192
};

class P2Pro {
public:
    P2Pro();
    ~P2Pro();

    bool connect();
    void disconnect();

    bool get_frame(P2ProFrame& frame);

    void pseudo_color_set(int preview_path, PseudoColorTypes color_type);
    PseudoColorTypes pseudo_color_get(int preview_path = 0);
    
    void set_prop_tpd_params(PropTpdParams tpd_param, uint16_t value);
    uint16_t get_prop_tpd_params(PropTpdParams tpd_param);
    
    std::vector<uint8_t> get_device_info(DeviceInfoType dev_info);

    void preview_start();
    void preview_stop();

private:
    std::unique_ptr<USBAdapter> adapter;

    bool check_camera_ready();
    bool block_until_camera_ready(int timeout_ms = 5000);

    void standard_cmd_write(uint16_t cmd, uint32_t cmd_param = 0, const std::vector<uint8_t>& data = {0});
    std::vector<uint8_t> standard_cmd_read(uint16_t cmd, uint32_t cmd_param = 0, uint16_t data_len = 0);

    void long_cmd_write(uint16_t cmd, uint16_t p1, uint32_t p2, uint32_t p3 = 0, uint32_t p4 = 0);
    std::vector<uint8_t> long_cmd_read(uint16_t cmd, uint16_t p1, uint32_t p2 = 0, uint32_t p3 = 0, uint32_t data_len = 2);

    static constexpr uint16_t VID = 0x0BDA;
    static constexpr uint16_t PID = 0x5830;

    static constexpr uint16_t CMD_SET = 0x4000;
    static constexpr uint16_t CMD_GET = 0x0000;

    enum CmdCode : uint16_t {
        GET_DEVICE_INFO_CMD = 0x8405,
        PSEUDO_COLOR_CMD = 0x8409,
        PROP_TPD_PARAMS_CMD = 0x8514,
        PREVIEW_START_CMD = 0xc10f,
        PREVIEW_STOP_CMD = 0x020f
    };
};

#endif
