#include "wifi_handler.h"
#include "display_handler.h"

#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include <time.h>

static const char *TAG = "WIFI";

#define WIFI_USE_HARDCODED_FALLBACK 1
static const char *FALLBACK_SSID = "YourMomDoesntWorkHereBro";
static const char *FALLBACK_PASSWORD = "TheKingOfTheIronFistTournament";

typedef enum {
  WIFI_UI_STATE_DISCONNECTED = 0,
  WIFI_UI_STATE_CONNECTING,
  WIFI_UI_STATE_CONNECTED,
  WIFI_UI_STATE_FAILED,
} wifi_ui_state_t;

typedef struct {
  wifi_ui_state_t state;
  bool connected;
  int rssi;
  char ssid[64];
  char ip[16];
  char fail_reason[128];
} wifi_ui_snapshot_t;

static bool g_wifi_started = false;
static bool g_time_synced = false;
static wifi_ui_snapshot_t g_last_ui = {
    .state = WIFI_UI_STATE_DISCONNECTED,
    .connected = false,
    .rssi = -127,
};

static void fetch_time_utc(void) {
  ESP_LOGI("TIME", "Starting SNTP");

  esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, "pool.ntp.org");
  esp_sntp_init();

  time_t now = 0;
  struct tm timeinfo = {0};

  int retry = 0;
  const int retry_count = 5;

  while (timeinfo.tm_year < (2020 - 1900) && ++retry < retry_count) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    time(&now);
    localtime_r(&now, &timeinfo);
  }

  if (timeinfo.tm_year >= (2020 - 1900)) {
    ESP_LOGI("TIME", "Time synced");
  } else {
    ESP_LOGW("TIME", "Failed to fetch time");
  }
}

static void wifi_publish_connecting_once(void) {
  if (g_last_ui.state == WIFI_UI_STATE_CONNECTING) {
    return;
  }

  g_last_ui.state = WIFI_UI_STATE_CONNECTING;
  g_last_ui.connected = false;
  g_last_ui.rssi = -127;
  g_last_ui.ssid[0] = '\0';
  g_last_ui.ip[0] = '\0';
  g_last_ui.fail_reason[0] = '\0';
  display_handler_wifi_connecting();
}

static void wifi_publish_failed_once(const char *reason) {
  const char *safe_reason = reason ? reason : "Connection failed";

  if (g_last_ui.state == WIFI_UI_STATE_FAILED &&
      strcmp(g_last_ui.fail_reason, safe_reason) == 0) {
    return;
  }

  g_last_ui.state = WIFI_UI_STATE_FAILED;
  g_last_ui.connected = false;
  g_last_ui.rssi = -127;
  g_last_ui.ssid[0] = '\0';
  g_last_ui.ip[0] = '\0';
  snprintf(g_last_ui.fail_reason, sizeof(g_last_ui.fail_reason), "%s",
           safe_reason);
  display_handler_wifi_failed(safe_reason);
}

static void wifi_publish_status_once(bool connected, const char *ssid,
                                     const char *ip, int rssi) {
  const char *safe_ssid = ssid ? ssid : "";
  const char *safe_ip = ip ? ip : "";

  if (g_last_ui.state == WIFI_UI_STATE_CONNECTED && g_last_ui.connected == connected &&
      g_last_ui.rssi == rssi && strcmp(g_last_ui.ssid, safe_ssid) == 0 &&
      strcmp(g_last_ui.ip, safe_ip) == 0) {
    return;
  }

  g_last_ui.state = connected ? WIFI_UI_STATE_CONNECTED
                              : WIFI_UI_STATE_DISCONNECTED;
  g_last_ui.connected = connected;
  g_last_ui.rssi = rssi;
  snprintf(g_last_ui.ssid, sizeof(g_last_ui.ssid), "%s", safe_ssid);
  snprintf(g_last_ui.ip, sizeof(g_last_ui.ip), "%s", safe_ip);
  g_last_ui.fail_reason[0] = '\0';
  display_handler_wifi_status(connected, ssid, ip, rssi);
}

static bool wifi_apply_config_and_connect(const char *ssid,
                                          const char *password) {
  if (!ssid || !password || strlen(ssid) == 0 || strlen(password) == 0) {
    wifi_publish_failed_once("SSID or password is empty");
    return false;
  }

  wifi_config_t wificonf = {0};
  snprintf((char *)wificonf.sta.ssid, sizeof(wificonf.sta.ssid), "%s", ssid);
  snprintf((char *)wificonf.sta.password, sizeof(wificonf.sta.password), "%s",
           password);
  wificonf.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  wificonf.sta.pmf_cfg.capable = true;
  wificonf.sta.pmf_cfg.required = false;

  wifi_publish_connecting_once();

  esp_err_t err = esp_wifi_disconnect();
  if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_CONNECT &&
      err != ESP_ERR_WIFI_CONN) {
    ESP_LOGW(TAG, "esp_wifi_disconnect returned: %s", esp_err_to_name(err));
  }

  err = esp_wifi_set_config(WIFI_IF_STA, &wificonf);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_wifi_set_config failed: %s", esp_err_to_name(err));
    wifi_publish_failed_once("Failed to apply WiFi config");
    return false;
  }

  if (!g_wifi_started) {
    err = esp_wifi_start();
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_wifi_start failed: %s", esp_err_to_name(err));
      wifi_publish_failed_once("Failed to start WiFi");
      return false;
    }
    g_wifi_started = true;
  }

  err = esp_wifi_connect();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_wifi_connect failed: %s", esp_err_to_name(err));
    wifi_publish_failed_once("Failed to start WiFi connection");
    return false;
  }

  return true;
}

void wh_start(void *args) {
  (void)args;

  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t wifiinitcfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&wifiinitcfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

#if WIFI_USE_HARDCODED_FALLBACK
  wifi_apply_config_and_connect(FALLBACK_SSID, FALLBACK_PASSWORD);
#endif

  esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  esp_netif_ip_info_t ip_info;
  wifi_ap_record_t ap_info;

  while (1) {
    char requested_ssid[64] = {0};
    char requested_password[64] = {0};

    if (display_handler_consume_wifi_connect_request(
            requested_ssid, sizeof(requested_ssid), requested_password,
            sizeof(requested_password))) {
      ESP_LOGI(TAG, "Received UI WiFi request for SSID: %s", requested_ssid);
      wifi_apply_config_and_connect(requested_ssid, requested_password);
      g_time_synced = false;
    }

    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
      if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        if (ip_info.ip.addr != 0) {
          char ip_str[16];
          snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
          wifi_publish_status_once(true, (const char *)ap_info.ssid, ip_str,
                                   ap_info.rssi);

          if (!g_time_synced) {
            fetch_time_utc();
            g_time_synced = true;
          }
        } else {
          wifi_publish_connecting_once();
        }
      } else {
        wifi_publish_connecting_once();
      }
    } else {
      wifi_publish_status_once(false, NULL, NULL, -100);
      g_time_synced = false;
    }

    vTaskDelay(pdMS_TO_TICKS(250));
  }
}

void wh_dispose(void) {}
