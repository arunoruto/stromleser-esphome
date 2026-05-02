#include "shelly_emulator.h"
#include "rpc_handlers.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#include <cJSON.h>
#include <cstdio>
#include <cstring>
#include <ctime>

#include <esp_system.h>
#include <esp_mac.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <mdns.h>
#include <lwip/sockets.h>
#include <driver/temperature_sensor.h>

namespace esphome {
namespace shelly_emulator {

static const char *TAG = "shelly_emulator";

// ============================================================================
// Sensor data accessors
// ============================================================================

float ShellyEmulator::get_current_power() const {
  if (power_sensor_ && power_sensor_->has_state()) {
    return power_sensor_->state;
  }
  return 0.0f;
}

float ShellyEmulator::get_energy_consumed() const {
  if (energy_consumed_sensor_ && energy_consumed_sensor_->has_state()) {
    return energy_consumed_sensor_->state;
  }
  return 0.0f;
}

float ShellyEmulator::get_energy_delivered() const {
  if (energy_delivered_sensor_ && energy_delivered_sensor_->has_state()) {
    return energy_delivered_sensor_->state;
  }
  return 0.0f;
}

void ShellyEmulator::get_phase_power(float *a, float *b, float *c) const {
  float total = get_current_power() + (power_offset_ * phase_count_);

  switch (phase_count_) {
    case 1:
      *a = total;
      *b = 0.0f;
      *c = 0.0f;
      break;
    case 3:
    default:
      *a = total / 3.0f;
      *b = total / 3.0f;
      *c = total / 3.0f;
      break;
  }
}

void ShellyEmulator::get_phase_energy(
    float *cons_a, float *cons_b, float *cons_c,
    float *del_a,  float *del_b,  float *del_c) const {
  float consumed = get_energy_consumed();
  float delivered = get_energy_delivered();

  switch (phase_count_) {
    case 1:
      *cons_a = consumed;
      *cons_b = 0.0f;
      *cons_c = 0.0f;
      *del_a  = delivered;
      *del_b  = 0.0f;
      *del_c  = 0.0f;
      break;
    case 3:
    default:
      *cons_a = consumed / 3.0f;
      *cons_b = consumed / 3.0f;
      *cons_c = consumed / 3.0f;
      *del_a  = delivered / 3.0f;
      *del_b  = delivered / 3.0f;
      *del_c  = delivered / 3.0f;
      break;
  }
}

float ShellyEmulator::get_temperature_c() const {
  return 40.0f;
}

int32_t ShellyEmulator::get_wifi_rssi() const {
  return wifi_rssi_;
}

// ============================================================================
// Network info
// ============================================================================

void ShellyEmulator::update_network_info() {
  ESP_LOGD(TAG, "update_network_info: begin");

  uint8_t mac[6];
  esp_err_t mac_err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
  if (mac_err == ESP_OK) {
    char mac_buf[13];
    snprintf(mac_buf, sizeof(mac_buf), "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    shelly_mac_ = mac_buf;
    shelly_name_ = std::string("shellypro3em-") + shelly_mac_;
  } else {
    ESP_LOGW(TAG, "update_network_info: esp_read_mac failed: %s", esp_err_to_name(mac_err));
  }

  esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (netif) {
    esp_netif_ip_info_t ip_info;
    esp_err_t ip_err = esp_netif_get_ip_info(netif, &ip_info);
    if (ip_err == ESP_OK) {
      char ip_buf[16];
      esp_ip4addr_ntoa(&ip_info.ip, ip_buf, sizeof(ip_buf));
      local_ip_ = ip_buf;
    } else {
      ESP_LOGW(TAG, "update_network_info: esp_netif_get_ip_info failed: %s",
               esp_err_to_name(ip_err));
    }
  } else {
    ESP_LOGW(TAG, "update_network_info: WIFI_STA_DEF netif not found");
  }

  wifi_config_t wifi_cfg;
  esp_err_t cfg_err = esp_wifi_get_config(WIFI_IF_STA, &wifi_cfg);
  if (cfg_err == ESP_OK) {
    size_t ssid_len = strnlen((const char *)wifi_cfg.sta.ssid, 32);
    wifi_ssid_.assign((const char *)wifi_cfg.sta.ssid, ssid_len);
  } else {
    ESP_LOGW(TAG, "update_network_info: esp_wifi_get_config failed: %s",
             esp_err_to_name(cfg_err));
  }

  wifi_ap_record_t ap_info;
  esp_err_t ap_err = esp_wifi_sta_get_ap_info(&ap_info);
  if (ap_err == ESP_OK) {
    wifi_rssi_ = ap_info.rssi;
    char bssid_buf[18];
    snprintf(bssid_buf, sizeof(bssid_buf),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             ap_info.bssid[0], ap_info.bssid[1], ap_info.bssid[2],
             ap_info.bssid[3], ap_info.bssid[4], ap_info.bssid[5]);
    wifi_bssid_ = bssid_buf;
  } else {
    ESP_LOGW(TAG, "update_network_info: esp_wifi_sta_get_ap_info failed: %s",
             esp_err_to_name(ap_err));
  }

  ESP_LOGD(TAG, "update_network_info: done, MAC=%s IP=%s",
           shelly_mac_.c_str(), local_ip_.c_str());
}

// ============================================================================
// RPC method dispatcher (shared by POST, WS, and UDP handlers)
// ============================================================================

static std::string dispatch_method(ShellyEmulator *comp, const std::string &method) {
  if (method == "Shelly.GetDeviceInfo")   return build_shelly_get_device_info(comp);
  if (method == "Shelly.GetComponents")   return build_shelly_get_components(comp);
  if (method == "Shelly.GetConfig")       return build_shelly_get_config(comp);
  if (method == "Shelly.GetStatus")       return build_shelly_get_status(comp);
  if (method == "EM.GetStatus")           return build_em_get_status(comp);
  if (method == "EMData.GetStatus")       return build_emdata_get_status(comp);
  if (method == "EM.GetConfig")           return build_em_get_config();
  if (method == "Sys.GetConfig")          return build_sys_get_config(comp);
  if (method == "Sys.GetStatus")          return build_sys_get_status(comp);
  if (method == "WiFi.GetStatus")         return build_wifi_get_status(comp);
  if (method == "Script.GetCode")         return std::string("{\"data\":\"\",\"left\":\"0\"}");
  if (method == "Script.List")            return std::string("{\"scripts\":[]}");
  return std::string("{}");
}

// ============================================================================
// cJSON helper to parse incoming RPC and call dispatcher + wrap
// ============================================================================

static std::string process_json_rpc(ShellyEmulator *comp, const char *body, int len) {
  cJSON *json = cJSON_ParseWithLength(body, len);
  if (!json) return "";

  cJSON *id_obj = cJSON_GetObjectItem(json, "id");
  cJSON *method_obj = cJSON_GetObjectItem(json, "method");
  cJSON *src_obj = cJSON_GetObjectItem(json, "src");

  int id = id_obj ? id_obj->valueint : 0;
  const char *method = method_obj ? method_obj->valuestring : "";
  const char *src = src_obj ? src_obj->valuestring : "EMPTY";

  std::string result = dispatch_method(comp, method);
  std::string response = rpc_wrap(id, src, result);

  ESP_LOGD(TAG, "RPC method=%s id=%d", method, id);

  cJSON_Delete(json);
  return response;
}

// ============================================================================
// HTTP GET handler — dispatches on URI path
// ============================================================================

esp_err_t ShellyEmulator::handle_get(httpd_req_t *req) {
  auto *comp = static_cast<ShellyEmulator *>(req->user_ctx);
  std::string uri(req->uri);
  std::string response;

  if (uri == "/shelly") {
    response = build_shelly_get_device_info(comp);
  } else if (uri == "/status") {
    response = build_shelly_get_status(comp);
  } else if (uri == "/rpc") {
    response = build_shelly_get_device_info(comp);
  } else if (uri == "/rpc/EM.GetStatus") {
    response = build_em_get_status(comp);
  } else if (uri == "/rpc/EMData.GetStatus") {
    response = build_emdata_get_status(comp);
  } else if (uri == "/rpc/EM.GetConfig") {
    response = build_em_get_config();
  } else if (uri == "/rpc/Shelly.GetDeviceInfo") {
    response = build_shelly_get_device_info(comp);
  } else if (uri == "/rpc/Shelly.GetStatus") {
    response = build_shelly_get_status(comp);
  } else if (uri == "/rpc/Shelly.GetConfig") {
    response = build_shelly_get_config(comp);
  } else if (uri == "/rpc/Shelly.GetComponents") {
    response = build_shelly_get_components(comp);
  } else if (uri == "/rpc/Sys.GetConfig") {
    response = build_sys_get_config(comp);
  } else if (uri == "/rpc/Sys.GetStatus") {
    response = build_sys_get_status(comp);
  } else if (uri == "/rpc/WiFi.GetStatus") {
    response = build_wifi_get_status(comp);
  } else {
    httpd_resp_send_404(req);
    return ESP_OK;
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, response.c_str(), response.length());
  return ESP_OK;
}

// ============================================================================
// HTTP POST /rpc handler — JSON-RPC
// ============================================================================

esp_err_t ShellyEmulator::handle_post_rpc(httpd_req_t *req) {
  auto *comp = static_cast<ShellyEmulator *>(req->user_ctx);
  std::string response = build_shelly_get_device_info(comp);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, response.c_str(), response.length());
  return ESP_OK;
}

// ============================================================================
// WebSocket /rpc handler
// ============================================================================

#ifdef CONFIG_HTTPD_WS_SUPPORT

esp_err_t ShellyEmulator::handle_ws(httpd_req_t *req) {
  auto *comp = static_cast<ShellyEmulator *>(req->user_ctx);

  if (req->method == HTTP_GET) {
    ESP_LOGI(TAG, "WebSocket client connected");
    return ESP_OK;
  }

  httpd_ws_frame_t ws_pkt;
  memset(&ws_pkt, 0, sizeof(ws_pkt));

  esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
  if (ret != ESP_OK) return ret;
  if (ws_pkt.len == 0) return ESP_OK;

  uint8_t *ws_buf = (uint8_t *)calloc(1, ws_pkt.len + 1);
  if (!ws_buf) return ESP_ERR_NO_MEM;

  ws_pkt.payload = ws_buf;
  ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
  if (ret != ESP_OK) {
    free(ws_buf);
    return ret;
  }
  ws_buf[ws_pkt.len] = '\0';

  if (ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
    std::string response = process_json_rpc(comp, (const char *)ws_buf, ws_pkt.len);

    if (!response.empty()) {
      httpd_ws_frame_t ws_resp;
      memset(&ws_resp, 0, sizeof(ws_resp));
      ws_resp.payload = (uint8_t *)response.c_str();
      ws_resp.len = response.length();
      ws_resp.type = HTTPD_WS_TYPE_TEXT;
      httpd_ws_send_frame(req, &ws_resp);
    }
  }

  free(ws_buf);
  return ESP_OK;
}

#else   // !CONFIG_HTTPD_WS_SUPPORT

esp_err_t ShellyEmulator::handle_ws(httpd_req_t *req) {
  return ESP_FAIL;
}

#endif  // CONFIG_HTTPD_WS_SUPPORT

// ============================================================================
// URI registration helpers
// ============================================================================

void ShellyEmulator::register_get_uri(const char *uri, httpd_method_t method) {
  httpd_uri_t h = {};
  h.uri = uri;
  h.method = method;
  h.handler = handle_get;
  h.user_ctx = this;
  esp_err_t err = httpd_register_uri_handler(http_server_, &h);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to register URI %s: %s", uri, esp_err_to_name(err));
  }
}

void ShellyEmulator::register_post_rpc_uri() {
  httpd_uri_t h = {};
  h.uri = "/rpc";
  h.method = HTTP_POST;
  h.handler = handle_post_rpc;
  h.user_ctx = this;
  esp_err_t err = httpd_register_uri_handler(http_server_, &h);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register POST /rpc: %s", esp_err_to_name(err));
  } else {
    ESP_LOGI(TAG, "POST /rpc registered");
  }
}

void ShellyEmulator::register_ws_uri() {
#ifdef CONFIG_HTTPD_WS_SUPPORT
  httpd_uri_t h = {};
  h.uri = "/rpc";
  h.method = HTTP_GET;
  h.handler = handle_ws;
  h.user_ctx = this;
  h.is_websocket = true;
  httpd_register_uri_handler(http_server_, &h);
  ESP_LOGI(TAG, "WebSocket /rpc registered");
#else
  ESP_LOGI(TAG, "WebSocket not available (CONFIG_HTTPD_WS_SUPPORT disabled)");
#endif
}

// ============================================================================
// HTTP server setup
// ============================================================================

void ShellyEmulator::setup_http_server() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.ctrl_port = 32768;
  config.max_uri_handlers = 16;
  config.lru_purge_enable = true;

  esp_err_t err = httpd_start(&http_server_, &config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
    return;
  }

  register_get_uri("/shelly");
  register_get_uri("/status");
  register_get_uri("/rpc");
  register_get_uri("/rpc/EM.GetStatus");
  register_get_uri("/rpc/EMData.GetStatus");
  register_get_uri("/rpc/EM.GetConfig");
  register_get_uri("/rpc/Shelly.GetDeviceInfo");
  register_get_uri("/rpc/Shelly.GetStatus");
  register_get_uri("/rpc/Shelly.GetConfig");
  register_get_uri("/rpc/Shelly.GetComponents");
  register_get_uri("/rpc/Sys.GetConfig");
  register_get_uri("/rpc/Sys.GetStatus");
  register_get_uri("/rpc/WiFi.GetStatus");

  register_post_rpc_uri();
  register_ws_uri();

  ESP_LOGI(TAG, "HTTP server started on port 80");
}

// ============================================================================
// mDNS setup
// ============================================================================
// mDNS setup
// ============================================================================

void ShellyEmulator::setup_mdns() {
  mdns_txt_item_t txt[4] = {
      {(char *)"arch", (char *)"esp32"},
      {(char *)"gen", (char *)"2"},
      {(char *)"fw_id", (char *)"20241011-114455/1.4.4-g6d2a586"},
      {(char *)"id", (char *)shelly_name_.c_str()},
  };

  esp_err_t err;

  err = mdns_service_add(shelly_name_.c_str(), "_http", "_tcp", 80, txt, 4);
  if (err == ESP_OK) {
    ESP_LOGI(TAG, "mDNS _http._tcp service added as '%s'", shelly_name_.c_str());
  } else {
    ESP_LOGW(TAG, "mDNS _http._tcp add failed: %s", esp_err_to_name(err));
  }

  err = mdns_service_add(shelly_name_.c_str(), "_shelly", "_tcp", 80, txt, 4);
  if (err == ESP_OK) {
    ESP_LOGI(TAG, "mDNS _shelly._tcp service added as '%s'", shelly_name_.c_str());
  } else {
    ESP_LOGW(TAG, "mDNS _shelly._tcp add failed: %s", esp_err_to_name(err));
  }
}

// ============================================================================
// UDP RPC
// ============================================================================

void ShellyEmulator::setup_udp_rpc() {
  udp_socket_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  if (udp_socket_ < 0) {
    ESP_LOGW(TAG, "Failed to create UDP socket");
    return;
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(udp_port_);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (::bind(udp_socket_, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    ESP_LOGW(TAG, "Failed to bind UDP port %d", udp_port_);
    ::close(udp_socket_);
    udp_socket_ = -1;
    return;
  }

  int flags = ::fcntl(udp_socket_, F_GETFL, 0);
  ::fcntl(udp_socket_, F_SETFL, flags | O_NONBLOCK);

  ESP_LOGI(TAG, "UDP RPC listener on port %d", udp_port_);
}

void ShellyEmulator::check_udp_packets() {
  if (udp_socket_ < 0) return;

  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(udp_socket_, &readfds);

  struct timeval tv = {0, 0};
  if (::select(udp_socket_ + 1, &readfds, nullptr, nullptr, &tv) <= 0) return;

  char buf[2048];
  struct sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);

  int len = ::recvfrom(udp_socket_, buf, sizeof(buf) - 1, 0,
                       (struct sockaddr *)&client_addr, &client_len);
  if (len <= 0) return;
  buf[len] = '\0';

  ESP_LOGD(TAG, "UDP RPC received %d bytes from %s:%d",
           len, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

  std::string response = process_json_rpc(this, buf, len);
  if (response.empty()) return;

  ::sendto(udp_socket_, response.c_str(), response.length(), 0,
           (struct sockaddr *)&client_addr, client_len);
}

// ============================================================================
// Component lifecycle
// ============================================================================

void ShellyEmulator::setup() {
  ESP_LOGI(TAG, "[1/4] setup: network info");
  update_network_info();
  ESP_LOGI(TAG, "[1/4] Device: %s  MAC: %s  IP: %s",
           shelly_name_.c_str(), shelly_mac_.c_str(), local_ip_.c_str());
  setup_pending_ = true;
  ESP_LOGI(TAG, "[1/4] setup done, deferring HTTP/mDNS/UDP to loop()");
}

void ShellyEmulator::deferred_setup() {
  ESP_LOGI(TAG, "[2/4] HTTP server starting...");
  setup_http_server();

  ESP_LOGI(TAG, "[3/4] mDNS starting...");
  setup_mdns();

  ESP_LOGI(TAG, "[4/4] UDP RPC starting...");
  setup_udp_rpc();

  ESP_LOGI(TAG, "ShellyEmulator ready — all services active");
}

void ShellyEmulator::loop() {
  if (setup_pending_ && millis() > 5000) {
    setup_pending_ = false;
    deferred_setup();
  }
  check_udp_packets();
}

void ShellyEmulator::dump_config() {
  ESP_LOGCONFIG(TAG, "Shelly 3EM Emulator:");
  ESP_LOGCONFIG(TAG, "  Device: %s", shelly_name_.c_str());
  ESP_LOGCONFIG(TAG, "  MAC: %s", shelly_mac_.c_str());
  ESP_LOGCONFIG(TAG, "  Phase count: %d", phase_count_);
  ESP_LOGCONFIG(TAG, "  Power offset: %.2f W", power_offset_);
  ESP_LOGCONFIG(TAG, "  UDP port: %d", udp_port_);
  ESP_LOGCONFIG(TAG, "  HTTP server: port 80");
  ESP_LOGCONFIG(TAG, "  mDNS: _shelly._tcp");
}

}  // namespace shelly_emulator
}  // namespace esphome
