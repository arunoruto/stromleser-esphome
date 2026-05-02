// Shelly 3EM Pro RPC response builders.
// JSON response formats based on the Shelly Gen2 API spec as implemented in
// Energy2Shelly_ESP (https://github.com/TheRealMoeder/Energy2Shelly_ESP).

#include "rpc_handlers.h"
#include "esphome/core/log.h"

#include <cJSON.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_mac.h>

#include <cstdio>
#include <ctime>

namespace esphome {
namespace shelly_emulator {

static const char *TAG = "shelly_emulator.rpc";

static void add_number_as_string(cJSON *obj, const char *key, float value) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%.2f", value);
  cJSON_AddStringToObject(obj, key, buf);
}

std::string rpc_wrap(int id, const std::string &src, const std::string &result) {
  char buf[4096];
  if (!src.empty() && src != "EMPTY") {
    snprintf(buf, sizeof(buf),
             "{\"id\":%d,\"src\":\"%s\",\"dst\":\"%s\",\"result\":%s}",
             id, src.c_str(), src.c_str(), result.c_str());
  } else {
    snprintf(buf, sizeof(buf),
             "{\"id\":%d,\"src\":\"%s\",\"result\":%s}",
             id, src.c_str(), result.c_str());
  }
  return std::string(buf);
}

static std::string to_string(cJSON *obj) {
  char *raw = cJSON_PrintUnformatted(obj);
  std::string result(raw);
  cJSON_free(raw);
  cJSON_Delete(obj);
  return result;
}

std::string build_em_get_status(ShellyEmulator *comp) {
  float pa, pb, pc;
  comp->get_phase_power(&pa, &pb, &pc);

  float total_power = pa + pb + pc;
  const float default_voltage = 230.0f;

  cJSON *root = cJSON_CreateObject();
  cJSON_AddNumberToObject(root, "id", 0);
  add_number_as_string(root, "a_current", pa / default_voltage);
  add_number_as_string(root, "a_voltage", pa > 0 ? default_voltage : 0);
  add_number_as_string(root, "a_act_power", pa);
  add_number_as_string(root, "a_aprt_power", pa);
  add_number_as_string(root, "a_pf", pa > 0 ? 1.0f : 0);
  add_number_as_string(root, "a_freq", 50.0f);
  add_number_as_string(root, "b_current", pb / default_voltage);
  add_number_as_string(root, "b_voltage", pb > 0 ? default_voltage : 0);
  add_number_as_string(root, "b_act_power", pb);
  add_number_as_string(root, "b_aprt_power", pb);
  add_number_as_string(root, "b_pf", pb > 0 ? 1.0f : 0);
  add_number_as_string(root, "b_freq", 50.0f);
  add_number_as_string(root, "c_current", pc / default_voltage);
  add_number_as_string(root, "c_voltage", pc > 0 ? default_voltage : 0);
  add_number_as_string(root, "c_act_power", pc);
  add_number_as_string(root, "c_aprt_power", pc);
  add_number_as_string(root, "c_pf", pc > 0 ? 1.0f : 0);
  add_number_as_string(root, "c_freq", 50.0f);
  cJSON_AddNullToObject(root, "n_current");
  add_number_as_string(root, "total_current", total_power / default_voltage);
  add_number_as_string(root, "total_act_power", total_power);
  add_number_as_string(root, "total_aprt_power", total_power);
  cJSON *cal = cJSON_AddArrayToObject(root, "user_calibrated_phase");
  (void)cal;

  return to_string(root);
}

std::string build_emdata_get_status(ShellyEmulator *comp) {
  float ca, cb, cc, da, db, dc;
  comp->get_phase_energy(&ca, &cb, &cc, &da, &db, &dc);

  // Convert kWh to Wh (multiply by 1000) for Shelly API
  float total_cons = (ca + cb + cc) * 1000.0f;
  float total_del = (da + db + dc) * 1000.0f;

  cJSON *root = cJSON_CreateObject();
  cJSON_AddNumberToObject(root, "id", 0);
  add_number_as_string(root, "a_total_act_energy", ca * 1000.0f);
  add_number_as_string(root, "a_total_act_ret_energy", da * 1000.0f);
  add_number_as_string(root, "b_total_act_energy", cb * 1000.0f);
  add_number_as_string(root, "b_total_act_ret_energy", db * 1000.0f);
  add_number_as_string(root, "c_total_act_energy", cc * 1000.0f);
  add_number_as_string(root, "c_total_act_ret_energy", dc * 1000.0f);
  add_number_as_string(root, "total_act", total_cons);
  add_number_as_string(root, "total_act_ret", total_del);

  return to_string(root);
}

std::string build_shelly_get_device_info(ShellyEmulator *comp) {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "id", comp->get_shelly_name().c_str());
  cJSON_AddStringToObject(root, "mac", comp->get_shelly_mac().c_str());
  cJSON_AddNumberToObject(root, "slot", 1);
  cJSON_AddStringToObject(root, "model", "SPEM-003CEBEU");
  cJSON_AddNumberToObject(root, "gen", 2);
  cJSON_AddStringToObject(root, "fw_id", "20241011-114455/1.4.4-g6d2a586");
  cJSON_AddStringToObject(root, "ver", "1.4.4");
  cJSON_AddStringToObject(root, "app", "Pro3EM");
  cJSON_AddFalseToObject(root, "auth_en");
  cJSON_AddNullToObject(root, "auth_domain");
  cJSON_AddStringToObject(root, "profile", "triphase");

  return to_string(root);
}

std::string build_em_get_config() {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddNumberToObject(root, "id", 0);
  cJSON_AddNullToObject(root, "name");
  cJSON_AddStringToObject(root, "blink_mode_selector", "active_energy");
  cJSON_AddStringToObject(root, "phase_selector", "a");
  cJSON_AddTrueToObject(root, "monitor_phase_sequence");
  cJSON *rev = cJSON_AddObjectToObject(root, "reverse");
  (void)rev;
  cJSON_AddStringToObject(root, "ct_type", "120A");

  return to_string(root);
}

std::string build_sys_get_config(ShellyEmulator *comp) {
  cJSON *root = cJSON_CreateObject();

  cJSON *device = cJSON_AddObjectToObject(root, "device");
  cJSON_AddStringToObject(device, "name", comp->get_shelly_name().c_str());
  cJSON_AddStringToObject(device, "mac", comp->get_shelly_mac().c_str());
  cJSON_AddStringToObject(device, "fw_id", "20241011-114455/1.4.4-g6d2a586");
  cJSON_AddFalseToObject(device, "eco_mode");
  cJSON_AddStringToObject(device, "profile", "triphase");
  cJSON_AddFalseToObject(device, "discoverable");

  cJSON *location = cJSON_AddObjectToObject(root, "location");
  cJSON_AddStringToObject(location, "tz", "Europe/Berlin");
  cJSON_AddNumberToObject(location, "lat", 54.306);
  cJSON_AddNumberToObject(location, "lon", 9.663);

  cJSON *debug = cJSON_AddObjectToObject(root, "debug");
  cJSON *dbg_mqtt = cJSON_AddObjectToObject(debug, "mqtt");
  cJSON_AddFalseToObject(dbg_mqtt, "enable");
  cJSON *dbg_ws = cJSON_AddObjectToObject(debug, "websocket");
  cJSON_AddFalseToObject(dbg_ws, "enable");
  cJSON *dbg_udp = cJSON_AddObjectToObject(debug, "udp");
  cJSON_AddNullToObject(dbg_udp, "addr");

  cJSON *ui = cJSON_AddObjectToObject(root, "ui_data");
  (void)ui;

  cJSON *rpc_udp = cJSON_AddObjectToObject(root, "rpc_udp");
  cJSON_AddStringToObject(rpc_udp, "dst_addr", comp->get_local_ip().c_str());
  cJSON_AddNullToObject(rpc_udp, "listen_port");

  cJSON *sntp = cJSON_AddObjectToObject(root, "sntp");
  cJSON_AddNullToObject(sntp, "server");

  cJSON_AddNumberToObject(root, "cfg_rev", 10);

  return to_string(root);
}

std::string build_sys_get_status(ShellyEmulator *comp) {
  time_t now;
  time(&now);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  char time_buf[8];
  strftime(time_buf, sizeof(time_buf), "%H:%M", &timeinfo);

  uint32_t free_heap = esp_get_free_heap_size();
  uint32_t flash_size = 4 * 1024 * 1024;
  int64_t uptime_s = esp_timer_get_time() / 1000000;

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "mac", comp->get_shelly_mac().c_str());
  cJSON_AddFalseToObject(root, "restart_required");
  cJSON_AddStringToObject(root, "time", time_buf);
  cJSON_AddNumberToObject(root, "unixtime", (double)now);
  cJSON_AddNumberToObject(root, "last_sync_ts", (double)now);
  cJSON_AddNumberToObject(root, "uptime", (double)uptime_s);
  cJSON_AddNumberToObject(root, "ram_size", (double)free_heap);
  cJSON_AddNumberToObject(root, "ram_free", (double)free_heap);
  cJSON_AddNumberToObject(root, "fs_size", (double)flash_size);
  cJSON_AddNumberToObject(root, "fs_free", (double)(flash_size / 4));
  cJSON_AddNumberToObject(root, "cfg_rev", 10);
  cJSON_AddNumberToObject(root, "kvs_rev", 2725);
  cJSON_AddNumberToObject(root, "schedule_rev", 0);
  cJSON_AddNumberToObject(root, "webhook_rev", 0);
  cJSON_AddNumberToObject(root, "btrelay_rev", 0);

  cJSON *updates = cJSON_AddObjectToObject(root, "available_updates");
  cJSON *beta = cJSON_AddObjectToObject(updates, "beta");
  cJSON_AddStringToObject(beta, "version", "1.7.5-beta1");

  return to_string(root);
}

std::string build_shelly_get_config(ShellyEmulator *comp) {
  cJSON *root = cJSON_CreateObject();

  cJSON *ble = cJSON_AddObjectToObject(root, "ble");
  cJSON_AddFalseToObject(ble, "enable");

  cJSON *cloud = cJSON_AddObjectToObject(root, "cloud");
  cJSON_AddFalseToObject(cloud, "enable");
  cJSON_AddNullToObject(cloud, "server");

  std::string em_config = build_em_get_config();
  // Parse and embed the raw JSON
  cJSON *em_parsed = cJSON_Parse(em_config.c_str());
  if (em_parsed) {
    cJSON_AddItemToObject(root, "em:0", em_parsed);
  }

  std::string sys_config = build_sys_get_config(comp);
  cJSON *sys_parsed = cJSON_Parse(sys_config.c_str());
  if (sys_parsed) {
    cJSON_AddItemToObject(root, "sys", sys_parsed);
  }

  cJSON *wifi = cJSON_AddObjectToObject(root, "wifi");
  cJSON *sta = cJSON_AddObjectToObject(wifi, "sta");
  cJSON_AddStringToObject(sta, "ssid", comp->get_wifi_ssid().c_str());
  cJSON_AddFalseToObject(sta, "is_open");
  cJSON_AddTrueToObject(sta, "enable");
  cJSON_AddStringToObject(sta, "ipv4mode", "dhcp");
  cJSON_AddStringToObject(sta, "ip", comp->get_local_ip().c_str());
  cJSON_AddStringToObject(sta, "netmask", "255.255.255.0");
  cJSON_AddStringToObject(sta, "gw", comp->get_local_ip().c_str());
  cJSON_AddStringToObject(sta, "nameserver", comp->get_local_ip().c_str());

  cJSON *ws = cJSON_AddObjectToObject(wifi, "ws");
  cJSON_AddFalseToObject(ws, "enable");
  cJSON_AddNullToObject(ws, "server");
  cJSON_AddStringToObject(ws, "ssl_ca", "ca.pem");

  return to_string(root);
}

std::string build_shelly_get_components(ShellyEmulator *comp) {
  cJSON *root = cJSON_CreateObject();

  cJSON *components = cJSON_AddArrayToObject(root, "components");

  cJSON *comp1 = cJSON_CreateObject();
  cJSON_AddStringToObject(comp1, "key", "em:0");

  std::string em_status = build_em_get_status(comp);
  cJSON *em_status_obj = cJSON_Parse(em_status.c_str());
  if (em_status_obj) {
    cJSON_AddItemToObject(comp1, "status", em_status_obj);
  }

  std::string em_config = build_em_get_config();
  cJSON *em_config_obj = cJSON_Parse(em_config.c_str());
  if (em_config_obj) {
    cJSON_AddItemToObject(comp1, "config", em_config_obj);
  }

  cJSON_AddItemToArray(components, comp1);

  cJSON *comp2 = cJSON_CreateObject();
  cJSON_AddStringToObject(comp2, "key", "emdata:0");

  std::string emdata_status = build_emdata_get_status(comp);
  cJSON *emdata_status_obj = cJSON_Parse(emdata_status.c_str());
  if (emdata_status_obj) {
    cJSON_AddItemToObject(comp2, "status", emdata_status_obj);
  }

  cJSON *emdata_config = cJSON_AddObjectToObject(comp2, "config");
  (void)emdata_config;

  cJSON_AddItemToArray(components, comp2);

  cJSON_AddNumberToObject(root, "cfg_rev", 1);
  cJSON_AddNumberToObject(root, "offset", 0);
  cJSON_AddNumberToObject(root, "total", 2);

  return to_string(root);
}

std::string build_shelly_get_status(ShellyEmulator *comp) {
  cJSON *root = cJSON_CreateObject();

  cJSON *ble = cJSON_AddObjectToObject(root, "ble");
  (void)ble;
  cJSON *bthome = cJSON_AddObjectToObject(root, "bthome");
  (void)bthome;

  cJSON *cloud = cJSON_AddObjectToObject(root, "cloud");
  cJSON_AddFalseToObject(cloud, "connected");

  std::string em_status = build_em_get_status(comp);
  cJSON *em_status_obj = cJSON_Parse(em_status.c_str());
  if (em_status_obj) {
    cJSON_AddItemToObject(root, "em:0", em_status_obj);
  }

  std::string emdata_status = build_emdata_get_status(comp);
  cJSON *emdata_status_obj = cJSON_Parse(emdata_status.c_str());
  if (emdata_status_obj) {
    cJSON_AddItemToObject(root, "emdata:0", emdata_status_obj);
  }

  cJSON *eth = cJSON_AddObjectToObject(root, "eth");
  cJSON_AddNullToObject(eth, "ip");
  cJSON_AddNullToObject(eth, "ip6");

  cJSON *modbus = cJSON_AddObjectToObject(root, "modbus");
  (void)modbus;

  cJSON *mqtt = cJSON_AddObjectToObject(root, "mqtt");
  cJSON_AddFalseToObject(mqtt, "connected");

  std::string sys_status = build_sys_get_status(comp);
  cJSON *sys_status_obj = cJSON_Parse(sys_status.c_str());
  if (sys_status_obj) {
    cJSON_AddItemToObject(root, "sys", sys_status_obj);
  }

  cJSON *temp = cJSON_AddObjectToObject(root, "temperature:0");
  cJSON_AddNumberToObject(temp, "id", 0);
  float temperature = comp->get_temperature_c();
  add_number_as_string(temp, "tC", temperature);
  add_number_as_string(temp, "tF", (temperature * 9.0f / 5.0f) + 32.0f);

  cJSON *wifi = cJSON_AddObjectToObject(root, "wifi");
  cJSON_AddStringToObject(wifi, "sta_ip", comp->get_local_ip().c_str());
  cJSON_AddStringToObject(wifi, "status",
                           comp->get_wifi_ssid().empty() ? "connecting" : "got ip");
  cJSON_AddStringToObject(wifi, "ssid",
                           comp->get_wifi_ssid().empty() ? "null" : comp->get_wifi_ssid().c_str());
  cJSON_AddStringToObject(wifi, "bssid",
                           comp->get_wifi_bssid().empty() ? "null" : comp->get_wifi_bssid().c_str());
  cJSON_AddNumberToObject(wifi, "rssi", comp->get_wifi_rssi());

  cJSON *sta_ip6 = cJSON_AddArrayToObject(wifi, "sta_ip6");
  (void)sta_ip6;

  cJSON *ws = cJSON_AddObjectToObject(root, "ws");
  cJSON_AddFalseToObject(ws, "connected");

  return to_string(root);
}

std::string build_wifi_get_status(ShellyEmulator *comp) {
  cJSON *root = cJSON_CreateObject();
  bool connected = !comp->get_wifi_ssid().empty();
  cJSON_AddStringToObject(root, "sta_ip", connected ? comp->get_local_ip().c_str() : "null");
  cJSON_AddStringToObject(root, "status", connected ? "got ip" : "connecting");
  cJSON_AddStringToObject(root, "ssid", connected ? comp->get_wifi_ssid().c_str() : "null");
  cJSON_AddStringToObject(root, "bssid", connected ? comp->get_wifi_bssid().c_str() : "null");
  cJSON_AddNumberToObject(root, "rssi", comp->get_wifi_rssi());
  cJSON_AddNumberToObject(root, "ap_client_count", 0);

  return to_string(root);
}

}  // namespace shelly_emulator
}  // namespace esphome
