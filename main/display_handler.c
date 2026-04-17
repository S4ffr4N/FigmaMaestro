#include "display_handler.h"
#include "gt911.h"
#include "lvgl_port.h"
#include "misc/lv_color.h"
#include "rgb_lcd_port.h"
#include "text_contents.h"
#include "ui.h"
#include "widgets/lv_label.h"

#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <time.h>

static UI g_ui;
static const char *TAG = "display_handler";

extern const lv_font_t notosans_14;

static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_touch_handle_t tp_handle = NULL;

static char screen_text[DISPLAY_MAX_CHAR_ROWS * DISPLAY_MAX_CHAR_PER_ROW] = {0};
static char model_info[87] = {0};
static char iso_string[20] = {0};
static char mem_info[91] = {0};

static char *get_iso_time_string(void) {
  time_t epoch = time(NULL);
  struct tm *tm = gmtime(&epoch);
  if (tm) {
    int year = tm->tm_year + 1900;
    int month = tm->tm_mon + 1;
    int day = tm->tm_mday;
    int hour = tm->tm_hour;
    int min = tm->tm_min;
    int sec = tm->tm_sec;
    if (snprintf(iso_string, 20, "%04d-%02d-%02dT%02d:%02d:%02d", year, month,
                 day, hour, min, sec) < 0) {
      ESP_LOGW(TAG, "Failed to parse current time");
      memset(iso_string, 0, sizeof(iso_string));
      snprintf(iso_string, sizeof(iso_string), "N/A");
    }
  } else {
    ESP_LOGW(TAG, "Failed to create tm struct from epoch");
    snprintf(iso_string, sizeof(iso_string), "N/A");
  }

  return iso_string;
}

void display_handler_wifi_status(bool connected, const char *ssid,
                                 const char *ip, int rssi) {
  if (lvgl_port_lock(-1)) {
    ui_set_wifi_status(&g_ui, connected, ssid, ip, rssi);
    lvgl_port_unlock();
  }
}

void display_handler_wifi_connecting(void) {
  if (lvgl_port_lock(-1)) {
    ui_set_wifi_connecting(&g_ui);
    lvgl_port_unlock();
  }
}

void display_handler_wifi_failed(const char *reason) {
  if (lvgl_port_lock(-1)) {
    ui_set_wifi_failed(&g_ui, reason);
    lvgl_port_unlock();
  }
}

bool display_handler_consume_wifi_connect_request(char *ssid, unsigned ssid_sz,
                                                  char *password,
                                                  unsigned password_sz) {
  bool ret = false;
  if (lvgl_port_lock(-1)) {
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

  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);

  snprintf(mem_info, sizeof(mem_info),
           "Total: %2.2fkB\n"
           "Internal: %2.2fkB\n"
           "External: %2.2fkB\n"
           "Largest free block: %u bytes",
           (float)esp_get_free_heap_size() / 1024,
           (float)heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024,
           (float)heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024,
           heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

  char intro[] = "----------------------------------------------------\n"
                 "-------------------- ESPMaestro --------------------\n"
                 "----------------------------------------------------\n";

  snprintf(screen_text, sizeof(screen_text),
           "Hello Boss \n%s\nSystem Time: %s\n%s\nMemory %s\n\n Click the "
           "buttons for more FAKTA",
           intro, get_iso_time_string(), model_info, mem_info);

  if (lvgl_port_lock(-1)) {
    ui_init(&g_ui);
    ui_show_home_screen(&g_ui);
    lvgl_port_unlock();
  }

  ESP_LOGI(TAG, "UI initialized, starting loop..");
  TickType_t x_last_wake = xTaskGetTickCount();
  const TickType_t x_freq = pdMS_TO_TICKS(1000);
  size_t counter = 0;

  while (1) {
    counter++;
    ESP_LOGI(TAG, "System tick #%zu!", counter);

    if (lvgl_port_lock(-1)) {
      ui_tick(&g_ui);
      lvgl_port_unlock();
    }
    vTaskDelayUntil(&x_last_wake, x_freq);
  }
}
