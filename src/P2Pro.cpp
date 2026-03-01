#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <windows.h>
#endif
#include "P2Pro.hpp"
#include <algorithm>
#ifdef __APPLE__
#include "MacOSAdapter.hpp"
#elif defined(_WIN32)
#include "WindowsAdapter.hpp"
#else
#include "LinuxAdapter.hpp"
#endif
#include "ColorConversion.hpp"
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

void dprintf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    fflush(stdout);
}

// Helper to handle endianness for 16 and 32 bit values if needed, 
// though we'll mostly use manual packing to match the python struct.pack calls.

P2Pro::P2Pro() {
#ifdef __APPLE__
    adapter = std::make_unique<MacOSAdapter>();
#elif defined(_WIN32)
    adapter = std::make_unique<WindowsAdapter>();
#else
    adapter = std::make_unique<LinuxAdapter>();
#endif
    is_swapped = false;
    swap_vote = 0;
    frames_since_connect = 0;
}

P2Pro::~P2Pro() {
    disconnect();
}

bool P2Pro::connect() {
    is_swapped = false;
    swap_vote = 0;
    frames_since_connect = 0;

    // 1. Then try to open video stream FIRST (as required on Windows and some other platforms)
    if (!adapter->open_video()) {
        dprintf("P2Pro::connect() - Failed to open video stream.\n");
        return false;
    }

    // 2. Try to connect via USB for control commands.
    if (!adapter->connect(VID, PID)) {
        dprintf("P2Pro::connect() - Failed to connect via USB for commands.\n");
        return false;
    }

    return true;
}

void P2Pro::disconnect() {
    adapter->disconnect();
    frames_since_connect = 0;
    swap_vote = 0;
}

bool P2Pro::is_connected() const {
    return adapter->is_connected();
}

bool P2Pro::get_frame(P2ProFrame &out_frame) {
    std::vector<uint8_t> raw_data;
    if (!adapter->read_frame(raw_data)) return false;

    // Expected size: 256 * 384 * 2 = 196608
    if (raw_data.size() < 196608) {
        dprintf("P2Pro::get_frame() - unexpected frame size %zu (need 196608)\n", raw_data.size());
        return false;
    }

    const size_t half_size = 256 * 192 * 2;
    uint8_t *pseudo_ptr;
    uint8_t *thermal_ptr;

    // We perform auto-detection for the first 30 frames after connection.
    // The frame layout shouldn't change during an active connection.
    if (frames_since_connect < 30) {
        long top_uv_diff = 0;
        long bot_uv_diff = 0;
        long top_y_var = 0;
        long bot_y_var = 0;

        // Sample pixels to determine which half is pseudo-color vs thermal
        for (size_t i = 0; i < half_size; i += 8) {
            top_uv_diff += std::abs((int) raw_data[i + 1] - (int) raw_data[i + 3]);
            top_y_var += std::abs((int) raw_data[i] - (int) raw_data[i + 2]);
        }
        for (size_t i = half_size; i < half_size * 2; i += 8) {
            bot_uv_diff += std::abs((int) raw_data[i + 1] - (int) raw_data[i + 3]);
            bot_y_var += std::abs((int) raw_data[i] - (int) raw_data[i + 2]);
        }

        // Weight the decision.
        // A real color image has significant UV variance.
        // Thermal data (Y16 interpreted as YUYV) has very low UV variance but high Y variance (L-bytes).
        // A threshold of 2000 for the sum of UV differences over 6144 samples is safer than 50.
        const long uv_threshold = 2000;
        
        if (std::abs(bot_uv_diff - top_uv_diff) > uv_threshold) {
            if (bot_uv_diff > top_uv_diff) swap_vote++; else swap_vote--;
        } else {
            // If UV is inconclusive, the thermal half (interpreted as YUYV) 
            // will have much higher Y-variance (L0 vs L1) than the pseudo-color half.
            if (top_y_var > bot_y_var) swap_vote++; else swap_vote--;
        }

        // Clamp vote
        if (swap_vote > 15) swap_vote = 15;
        if (swap_vote < -15) swap_vote = -15;

        bool new_swapped = (swap_vote > 0);
        if (frames_since_connect == 0 || new_swapped != is_swapped) {
            dprintf("P2Pro::get_frame() - Auto-detect (frame %d): %s (Top UV: %ld, Y: %ld | Bot UV: %ld, Y: %ld)\n",
                    frames_since_connect,
                    new_swapped ? "Swapped (Pseudo in bottom)" : "Standard (Pseudo in top)",
                    top_uv_diff, top_y_var, bot_uv_diff, bot_y_var);
            is_swapped = new_swapped;
        }
        frames_since_connect++;
        
        if (frames_since_connect == 30) {
            dprintf("P2Pro::get_frame() - Detection locked: %s\n", is_swapped ? "Swapped" : "Standard");
        }
    }

    if (is_swapped) {
        pseudo_ptr = raw_data.data() + half_size;
        thermal_ptr = raw_data.data();
    } else {
        pseudo_ptr = raw_data.data();
        thermal_ptr = raw_data.data() + half_size;
    }

    // YUYV to RGB
    out_frame.rgb.resize(256 * 192 * 3);
    ColorConversion::YUY2toRGB(pseudo_ptr, out_frame.rgb.data(), 256, 192);

    // Extract thermal data
    uint16_t *thermal_raw = (uint16_t *) thermal_ptr;
    out_frame.thermal.assign(thermal_raw, thermal_raw + (256 * 192));

    return true;
}

bool P2Pro::check_camera_ready() {
    uint8_t ret_val;
    bool success = adapter->control_transfer(0xC1, 0x44, 0x78, 0x200, &ret_val, 1, 1000);
    if (!success) return false;

    if ((ret_val & 1) == 0 && (ret_val & 2) == 0) {
        return true;
    }
    return false;
}

bool P2Pro::block_until_camera_ready(int timeout_ms) {
    auto start = std::chrono::steady_clock::now();
    while (true) {
        if (check_camera_ready()) return true;
        if (!adapter->is_connected()) return true;  // No USB — skip the wait
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() > timeout_ms) {
            return false;
        }
    }
}

void P2Pro::standard_cmd_write(uint16_t cmd, uint32_t cmd_param, const std::vector<uint8_t> &data) {
    uint32_t swapped_param = htonl(cmd_param);

    if (data.empty() || (data.size() == 1 && data[0] == 0)) {
        uint8_t d[8];
        d[0] = cmd & 0xFF;
        d[1] = (cmd >> 8) & 0xFF;
        memcpy(d + 2, &swapped_param, 4);
        d[6] = 0;
        d[7] = 0;
        adapter->control_transfer(0x41, 0x45, 0x78, 0x1d00, d, 8, 1000);
        block_until_camera_ready();
        return;
    }

    uint16_t dataLen = static_cast<uint16_t>(data.size());
    for (size_t i = 0; i < dataLen; i += 0x100) {
        size_t outer_chunk_size = std::min((size_t) 0x100, dataLen - i);

        uint8_t initial_data[8];
        initial_data[0] = cmd & 0xFF;
        initial_data[1] = (cmd >> 8) & 0xFF;
        uint32_t current_param = htonl(cmd_param + (uint32_t) i);
        memcpy(initial_data + 2, &current_param, 4);
        initial_data[6] = outer_chunk_size & 0xFF;
        initial_data[7] = (outer_chunk_size >> 8) & 0xFF;

        adapter->control_transfer(0x41, 0x45, 0x78, 0x9d00, initial_data, 8, 1000);
        block_until_camera_ready();

        for (size_t j = 0; j < outer_chunk_size; j += 0x40) {
            size_t inner_chunk_size = std::min((size_t) 0x40, outer_chunk_size - j);
            size_t to_send = outer_chunk_size - j;

            if (to_send <= 8) {
                adapter->control_transfer(0x41, 0x45, 0x78, 0x1d08 + (uint16_t) j,
                                          (unsigned char *) data.data() + i + j, (uint16_t) inner_chunk_size, 1000);
                block_until_camera_ready();
            } else if (to_send <= 64) {
                adapter->control_transfer(0x41, 0x45, 0x78, 0x9d08 + (uint16_t) j,
                                          (unsigned char *) data.data() + i + j, (uint16_t)(inner_chunk_size - 8),
                                          1000);
                adapter->control_transfer(0x41, 0x45, 0x78, 0x1d08 + (uint16_t) j + (uint16_t) to_send - 8,
                                          (unsigned char *) data.data() + i + j + inner_chunk_size - 8, 8, 1000);
                block_until_camera_ready();
            } else {
                adapter->control_transfer(0x41, 0x45, 0x78, 0x9d08 + (uint16_t) j,
                                          (unsigned char *) data.data() + i + j, (uint16_t) inner_chunk_size, 1000);
            }
        }
    }
}

std::vector<uint8_t> P2Pro::standard_cmd_read(uint16_t cmd, uint32_t cmd_param, uint16_t data_len) {
    if (data_len == 0) return {};

    std::vector<uint8_t> result;
    for (uint32_t i = 0; i < data_len; i += 0x100) {
        uint16_t to_read = (uint16_t) std::min((uint32_t) 0x100, (uint32_t) data_len - i);

        uint8_t initial_data[8];
        initial_data[0] = cmd & 0xFF;
        initial_data[1] = (cmd >> 8) & 0xFF;
        uint32_t current_param = htonl(cmd_param + i);
        memcpy(initial_data + 2, &current_param, 4);
        initial_data[6] = to_read & 0xFF;
        initial_data[7] = (to_read >> 8) & 0xFF;

        adapter->control_transfer(0x41, 0x45, 0x78, 0x1d00, initial_data, 8, 1000);
        block_until_camera_ready();

        std::vector<uint8_t> buffer(to_read);
        adapter->control_transfer(0xC1, 0x44, 0x78, 0x1d08, buffer.data(), to_read, 1000);
        result.insert(result.end(), buffer.begin(), buffer.end());
    }
    return result;
}

void P2Pro::long_cmd_write(uint16_t cmd, uint16_t p1, uint32_t p2, uint32_t p3, uint32_t p4) {
    uint8_t data1[8];
    data1[0] = cmd & 0xFF;
    data1[1] = (cmd >> 8) & 0xFF;
    uint16_t p1_swapped = htons(p1);
    uint32_t p2_swapped = htonl(p2);
    memcpy(data1 + 2, &p1_swapped, 2);
    memcpy(data1 + 4, &p2_swapped, 4);

    uint8_t data2[8];
    uint32_t p3_swapped = htonl(p3);
    uint32_t p4_swapped = htonl(p4);
    memcpy(data2, &p3_swapped, 4);
    memcpy(data2 + 4, &p4_swapped, 4);

    adapter->control_transfer(0x41, 0x45, 0x78, 0x9d00, data1, 8, 1000);
    adapter->control_transfer(0x41, 0x45, 0x78, 0x1d08, data2, 8, 1000);
    block_until_camera_ready();
}

std::vector<uint8_t> P2Pro::long_cmd_read(uint16_t cmd, uint16_t p1, uint32_t p2, uint32_t p3, uint32_t data_len) {
    uint8_t data1[8];
    data1[0] = cmd & 0xFF;
    data1[1] = (cmd >> 8) & 0xFF;
    uint16_t p1_swapped = htons(p1);
    uint32_t p2_swapped = htonl(p2);
    memcpy(data1 + 2, &p1_swapped, 2);
    memcpy(data1 + 4, &p2_swapped, 4);

    uint8_t data2[8];
    uint32_t p3_swapped = htonl(p3);
    uint32_t p4_swapped = htonl(data_len);
    memcpy(data2, &p3_swapped, 4);
    memcpy(data2 + 4, &p4_swapped, 4);

    adapter->control_transfer(0x41, 0x45, 0x78, 0x9d00, data1, 8, 1000);
    adapter->control_transfer(0x41, 0x45, 0x78, 0x1d08, data2, 8, 1000);
    block_until_camera_ready();

    std::vector<uint8_t> result(data_len);
    adapter->control_transfer(0xC1, 0x44, 0x78, 0x1d10, result.data(), (uint16_t) data_len, 1000);
    return result;
}

void P2Pro::pseudo_color_set(int preview_path, PseudoColorTypes color_type) {
    standard_cmd_write(CmdCode::PSEUDO_COLOR_CMD | CMD_SET, (uint32_t) preview_path, {(uint8_t) color_type});
}

PseudoColorTypes P2Pro::pseudo_color_get(int preview_path) {
    auto res = standard_cmd_read(CmdCode::PSEUDO_COLOR_CMD, (uint32_t) preview_path, 1);
    return static_cast<PseudoColorTypes>(res[0]);
}

void P2Pro::set_prop_tpd_params(PropTpdParams tpd_param, uint16_t value) {
    long_cmd_write(CmdCode::PROP_TPD_PARAMS_CMD | CMD_SET, (uint16_t) tpd_param, (uint32_t) value);
}

uint16_t P2Pro::get_prop_tpd_params(PropTpdParams tpd_param) {
    auto res = long_cmd_read(CmdCode::PROP_TPD_PARAMS_CMD, (uint16_t) tpd_param);
    uint16_t val;
    memcpy(&val, res.data(), 2);
    return ntohs(val);
}

std::vector<uint8_t> P2Pro::get_device_info(DeviceInfoType dev_info) {
    static const uint16_t lengths[] = {8, 8, 8, 26, 4, 50, 48, 16, 4};
    return standard_cmd_read(CmdCode::GET_DEVICE_INFO_CMD, (uint32_t) dev_info, lengths[(int) dev_info]);
}

void P2Pro::preview_start() {
    standard_cmd_write(CmdCode::PREVIEW_START_CMD);
}

void P2Pro::preview_stop() {
    standard_cmd_write(CmdCode::PREVIEW_STOP_CMD);
}