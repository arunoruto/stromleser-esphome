#pragma once

// Shelly 3EM Pro RPC response builders.
// JSON response formats based on the Shelly Gen2 API spec as implemented in
// Energy2Shelly_ESP (https://github.com/TheRealMoeder/Energy2Shelly_ESP).

#include <string>
#include "shelly_emulator.h"

namespace esphome {
namespace shelly_emulator {

std::string build_em_get_status(ShellyEmulator *comp);
std::string build_emdata_get_status(ShellyEmulator *comp);
std::string build_shelly_get_device_info(ShellyEmulator *comp);
std::string build_shelly_get_status(ShellyEmulator *comp);
std::string build_shelly_get_config(ShellyEmulator *comp);
std::string build_shelly_get_components(ShellyEmulator *comp);
std::string build_em_get_config();
std::string build_sys_get_config(ShellyEmulator *comp);
std::string build_sys_get_status(ShellyEmulator *comp);
std::string build_wifi_get_status(ShellyEmulator *comp);

std::string rpc_wrap(int id, const std::string &src, const std::string &result);

}  // namespace shelly_emulator
}  // namespace esphome
