#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

#include <string>
#include <esp_http_server.h>

namespace esphome {
namespace shelly_emulator {

class ShellyEmulator : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::WIFI + 1.0f; }

  void set_power_sensor(sensor::Sensor *s) { power_sensor_ = s; }
  void set_energy_consumed_sensor(sensor::Sensor *s) { energy_consumed_sensor_ = s; }
  void set_energy_delivered_sensor(sensor::Sensor *s) { energy_delivered_sensor_ = s; }

  void set_phase_count(uint8_t count) { phase_count_ = count; }
  void set_power_offset(float offset) { power_offset_ = offset; }
  void set_udp_port(uint16_t port) { udp_port_ = port; }

  float get_current_power() const;
  float get_energy_consumed() const;
  float get_energy_delivered() const;

  void get_phase_power(float *a, float *b, float *c) const;
  void get_phase_energy(float *cons_a, float *cons_b, float *cons_c,
                        float *del_a, float *del_b, float *del_c) const;

  const std::string &get_shelly_mac() const { return shelly_mac_; }
  const std::string &get_shelly_name() const { return shelly_name_; }
  const std::string &get_local_ip() const { return local_ip_; }
  uint8_t get_phase_count() const { return phase_count_; }

  float get_temperature_c() const;
  int32_t get_wifi_rssi() const;
  const std::string &get_wifi_ssid() const { return wifi_ssid_; }
  const std::string &get_wifi_bssid() const { return wifi_bssid_; }

  void update_network_info();

 protected:
  sensor::Sensor *power_sensor_{nullptr};
  sensor::Sensor *energy_consumed_sensor_{nullptr};
  sensor::Sensor *energy_delivered_sensor_{nullptr};

  uint8_t phase_count_{1};
  float power_offset_{0.0f};
  uint16_t udp_port_{2220};

  std::string shelly_mac_;
  std::string shelly_name_;
  std::string local_ip_;
  std::string wifi_ssid_;
  std::string wifi_bssid_;
  int32_t wifi_rssi_{0};

  httpd_handle_t http_server_{nullptr};
  int udp_socket_{-1};

  bool setup_pending_{true};
  void deferred_setup();

  void setup_http_server();
  void setup_mdns();
  void setup_udp_rpc();
  void check_udp_packets();

  static esp_err_t handle_get(httpd_req_t *req);
  static esp_err_t handle_post_rpc(httpd_req_t *req);
  static esp_err_t handle_ws(httpd_req_t *req);
  void register_get_uri(const char *uri, httpd_method_t method = HTTP_GET);
  void register_post_rpc_uri();
  void register_ws_uri();
};

}  // namespace shelly_emulator
}  // namespace esphome
