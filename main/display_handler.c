#include "display_handler.h"
#include "gt911.h"
#include "lvgl_port.h"
#include "rgb_lcd_port.h"
#include "ui.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static UI g_ui;
static const char *TAG = "display_handler";

static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_touch_handle_t tp_handle = NULL;

void display_handler_wifi_status(bool connected, const char *ssid,
                                 const char *ip, int rssi) {
  static bool last_connected = false;
  static int last_rssi = -127;
  static char last_ssid[64] = {0};
  static char last_ip[32] = {0};

  const char *safe_ssid = ssid ? ssid : "";
  const char *safe_ip = ip ? ip : "";

  if (last_connected == connected && last_rssi == rssi &&
      strcmp(last_ssid, safe_ssid) == 0 && strcmp(last_ip, safe_ip) == 0) {
    return;
  }

  last_connected = connected;
  last_rssi = rssi;
  snprintf(last_ssid, sizeof(last_ssid), "%s", safe_ssid);
  snprintf(last_ip, sizeof(last_ip), "%s", safe_ip);

  if (lvgl_port_lock(pdMS_TO_TICKS(20))) {
    ui_set_wifi_status(&g_ui, connected, ssid, ip, rssi);
    lvgl_port_unlock();
  }
}

void display_handler_wifi_connecting(void) {
    if (lvgl_port_lock(pdMS_TO_TICKS(20))) {
    ui_set_wifi_connecting(&g_ui);
    lvgl_port_unlock();
  }
}

void display_handler_wifi_failed(const char *reason) {
  static char last_reason[128] = {0};
  const char *safe_reason = reason ? reason : "Connection failed";

  if (strcmp(last_reason, safe_reason) == 0) {
    return;
  }
  snprintf(last_reason, sizeof(last_reason), "%s", safe_reason);

  if (lvgl_port_lock(pdMS_TO_TICKS(20))) {
    ui_set_wifi_failed(&g_ui, reason);
    lvgl_port_unlock();
  }
}

bool display_handler_consume_wifi_connect_request(char *ssid, unsigned ssid_sz,
                                                  char *password,
                                                  unsigned password_sz) {
  bool ret = false;
  if (lvgl_port_lock(pdMS_TO_TICKS(10))) {
    ret = ui_consume_wifi_connect_request(&g_ui, ssid, ssid_sz, password,
                                          password_sz);
    lvgl_port_unlock();
  }
  return ret;
}

int display_handler_init(DH *_DH) {
  (void)_DH;

  lv_init();

  tp_handle = touch_gt911_init();
  if (tp_handle == NULL) {
    ESP_LOGE(TAG, "Failed to initialize GT911 touch controller");
    return -1;
  }

  panel_handle = waveshare_esp32_s3_rgb_lcd_init();
  if (panel_handle == NULL) {
    ESP_LOGE(TAG, "Failed to initialize RGB LCD panel");
    return -1;
  }

  esp_err_t err = lvgl_port_init(panel_handle, tp_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "lvgl_port_init failed");
    return -1;
  }

  wavesahre_rgb_lcd_bl_on();

  ESP_LOGI(TAG, "Display handler initialized successfully");
  return 0;
}

void display_handler_work(void *_null_for_now) {
  (void)_null_for_now;

  if (lvgl_port_lock(-1)) {
    ui_init(&g_ui);
    ui_show_home_screen(&g_ui);
    lvgl_port_unlock();
  }

  ESP_LOGI(TAG, "UI initialized, starting loop");
  TickType_t x_last_wake = xTaskGetTickCount();
  const TickType_t x_freq = pdMS_TO_TICKS(1000);

  while (1) {
    if (lvgl_port_lock(pdMS_TO_TICKS(20))) {
      ui_tick(&g_ui);
      lvgl_port_unlock();
    }
    vTaskDelayUntil(&x_last_wake, x_freq);
  }
}
