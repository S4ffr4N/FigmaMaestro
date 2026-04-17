#ifndef __UI_H_
#define __UI_H_

#include "lvgl.h"
#include <stdbool.h>

typedef enum {
  UI_SCREEN_HOME = 0,
  UI_SCREEN_SETTINGS,
  UI_SCREEN_WIFI_CONFIG,
  UI_SCREEN_FACILITY_CONFIG,
  UI_SCREEN_DEVICE_INFO,
  UI_SCREEN_FACTS
} ui_screen_t;

typedef enum {
  UI_WIFI_STATE_DISCONNECTED = 0,
  UI_WIFI_STATE_CONNECTING,
  UI_WIFI_STATE_CONNECTED
} ui_wifi_state_t;

typedef struct {
  lv_obj_t *root;
  lv_obj_t *header;
  lv_obj_t *body;
  lv_obj_t *footer;

  lv_obj_t *header_home_btn;
  lv_obj_t *header_title;
  lv_obj_t *header_wifi_btn;
  lv_obj_t *footer_datetime_label;
  lv_obj_t *footer_status_label;

  lv_obj_t *wifi_status_box;
  lv_obj_t *ssid_ta;
  lv_obj_t *password_ta;
  lv_obj_t *password_toggle_btn;
  lv_obj_t *connect_btn;
  lv_obj_t *keyboard;

  lv_obj_t *facility_name_ta;
  lv_obj_t *country_ta;
  lv_obj_t *address_ta;
  lv_obj_t *city_ta;
  lv_obj_t *postal_code_ta;
  lv_obj_t *currency_ta;
  lv_obj_t *energy_zone_ta;
  lv_obj_t *solar_tilt_ta;
  lv_obj_t *solar_azimuth_ta;
  lv_obj_t *solar_size_ta;

  ui_screen_t current_screen;
  ui_wifi_state_t wifi_state;

  bool wifi_connected;
  bool show_password;

  int wifi_rssi;

  char wifi_status[128];
  char connected_ssid[64];
  char ip_address[32];
  char datetime_string[64];

  char pending_ssid[64];
  char pending_password[64];

  char facility_name[64];
  char country[32];
  char address[96];
  char city[48];
  char postal_code[24];
  char currency[16];
  char energy_zone[24];
  char solar_tilt[16];
  char solar_azimuth[16];
  char solar_size[16];

  bool wifi_connect_request_pending;
} UI;

void ui_init(UI *_UI);
void ui_show_home_screen(UI *_UI);
void ui_set_wifi_status(UI *_UI, bool _connected, const char *_ssid,
                        const char *_ip, int _rssi);
void ui_set_wifi_connecting(UI *_UI);
void ui_set_wifi_failed(UI *_UI, const char *_reason);
void ui_tick(UI *_UI);

bool ui_consume_wifi_connect_request(UI *_UI, char *_ssid, unsigned _ssid_sz,
                                     char *_password, unsigned _password_sz);

#endif
