#include "ui.h"
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *TAG = "UI";

static void ui_safe_copy(char *_dst, size_t _dst_sz, const char *_src);
static void ui_rebuild(UI *_UI);
static void ui_clear_containers(UI *_UI);
static void ui_build_header(UI *_UI);
static void ui_build_footer(UI *_UI);
static void ui_build_home(UI *_UI);
static void ui_build_settings(UI *_UI);
static void ui_build_wifi_config(UI *_UI);
static void ui_build_facility_config(UI *_UI);
static void ui_build_device_info(UI *_UI);
static void ui_build_facts(UI *_UI);

static lv_obj_t *ui_create_card(lv_obj_t *_parent, lv_coord_t _w,
                                lv_coord_t _h);
static lv_obj_t *ui_create_button(lv_obj_t *_parent, const char *_text,
                                  lv_color_t _bg, lv_coord_t _w, lv_coord_t _h);
static lv_obj_t *ui_create_menu_tile(lv_obj_t *_parent, const char *_title,
                                     const char *_subtitle, lv_color_t _bg,
                                     lv_event_cb_t _cb, UI *_UI);
static lv_obj_t *ui_create_input_row(UI *_UI, lv_obj_t *_parent,
                                     const char *_label,
                                     const char *_placeholder,
                                     lv_obj_t **_ta_out, bool _password);
static void ui_apply_dark_card_style(lv_obj_t *_obj);
static void ui_apply_dark_button_style(lv_obj_t *_obj, lv_color_t _bg);
static void ui_apply_input_style(lv_obj_t *_obj);
static void ui_update_footer_status(UI *_UI);
static void ui_update_footer_datetime(UI *_UI);
static void ui_update_wifi_header_indicator(UI *_UI);
static void ui_update_wifi_status_box(UI *_UI);
static void ui_update_connect_button(UI *_UI);
static void ui_update_datetime_string(UI *_UI);
static int ui_rssi_to_bars(int _rssi);
static lv_color_t ui_stack_color_for_watermark(UBaseType_t _words);
static const char *ui_stack_status_for_watermark(UBaseType_t _words);

static void home_btn_event_cb(lv_event_t *_event);
static void home_settings_event_cb(lv_event_t *_event);
static void home_facts_event_cb(lv_event_t *_event);
static void settings_wifi_event_cb(lv_event_t *_event);
static void settings_facility_event_cb(lv_event_t *_event);
static void settings_device_info_event_cb(lv_event_t *_event);
static void password_toggle_event_cb(lv_event_t *_event);
static void connect_event_cb(lv_event_t *_event);
static void textarea_changed_event_cb(lv_event_t *_event);
static void keyboard_event_cb(lv_event_t *_event);
static void textarea_focus_event_cb(lv_event_t *_event);
static void ui_hide_keyboard(UI *_UI);
static void ui_show_keyboard_for_textarea(UI *_UI, lv_obj_t *_ta);

static void ui_safe_copy(char *_dst, size_t _dst_sz, const char *_src) {
  if (!_dst || _dst_sz == 0)
    return;
  if (!_src)
    _src = "";
  snprintf(_dst, _dst_sz, "%s", _src);
}

static void ui_update_datetime_string(UI *_UI) {
  time_t now = time(NULL);
  struct tm tm_now;
  localtime_r(&now, &tm_now);

  snprintf(_UI->datetime_string, sizeof(_UI->datetime_string),
           "%04d-%02d-%02d %02d:%02d:%02d", tm_now.tm_year + 1900,
           tm_now.tm_mon + 1, tm_now.tm_mday, tm_now.tm_hour, tm_now.tm_min,
           tm_now.tm_sec);
}

static int ui_rssi_to_bars(int _rssi) {
  if (_rssi >= -55)
    return 4;
  if (_rssi >= -65)
    return 3;
  if (_rssi >= -75)
    return 2;
  if (_rssi >= -85)
    return 1;
  return 0;
}

static lv_color_t ui_stack_color_for_watermark(UBaseType_t _words) {
  if (_words <= 64)
    return lv_color_hex(0xDC2626);
  if (_words <= 160)
    return lv_color_hex(0xEAB308);
  return lv_color_hex(0x22C55E);
}

static const char *ui_stack_status_for_watermark(UBaseType_t _words) {
  if (_words <= 64)
    return "CRITICAL";
  if (_words <= 160)
    return "HIGH USAGE";
  return "OK";
}

static void ui_apply_dark_card_style(lv_obj_t *_obj) {
  lv_obj_set_style_radius(_obj, 18, 0);
  lv_obj_set_style_border_width(_obj, 1, 0);
  lv_obj_set_style_border_color(_obj, lv_color_hex(0x262626), 0);
  lv_obj_set_style_bg_color(_obj, lv_color_hex(0x171717), 0);
  lv_obj_set_style_bg_opa(_obj, LV_OPA_COVER, 0);
}

static void ui_apply_dark_button_style(lv_obj_t *_obj, lv_color_t _bg) {
  lv_obj_set_style_radius(_obj, 12, 0);
  lv_obj_set_style_border_width(_obj, 0, 0);
  lv_obj_set_style_bg_color(_obj, _bg, 0);
  lv_obj_set_style_bg_opa(_obj, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(_obj, lv_color_white(), 0);
}

static void ui_apply_input_style(lv_obj_t *_obj) {
  lv_obj_set_style_radius(_obj, 12, 0);
  lv_obj_set_style_border_width(_obj, 1, 0);
  lv_obj_set_style_border_color(_obj, lv_color_hex(0x262626), 0);
  lv_obj_set_style_bg_color(_obj, lv_color_hex(0x0A0A0A), 0);
  lv_obj_set_style_bg_opa(_obj, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(_obj, lv_color_white(), 0);
  lv_obj_set_style_pad_left(_obj, 12, 0);
  lv_obj_set_style_pad_right(_obj, 12, 0);
}

static lv_obj_t *ui_create_card(lv_obj_t *_parent, lv_coord_t _w,
                                lv_coord_t _h) {
  lv_obj_t *card = lv_obj_create(_parent);
  lv_obj_set_size(card, _w, _h);
  ui_apply_dark_card_style(card);
  return card;
}

static lv_obj_t *ui_create_button(lv_obj_t *_parent, const char *_text,
                                  lv_color_t _bg, lv_coord_t _w,
                                  lv_coord_t _h) {
  lv_obj_t *btn = lv_btn_create(_parent);
  lv_obj_set_size(btn, _w, _h);
  ui_apply_dark_button_style(btn, _bg);
  lv_obj_t *lbl = lv_label_create(btn);
  lv_label_set_text(lbl, _text);
  lv_obj_center(lbl);
  return btn;
}

static lv_obj_t *ui_create_menu_tile(lv_obj_t *_parent, const char *_title,
                                     const char *_subtitle, lv_color_t _bg,
                                     lv_event_cb_t _cb, UI *_UI) {
  lv_obj_t *tile = ui_create_card(_parent, 320, 180);
  lv_obj_set_style_bg_color(tile, _bg, 0);
  lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(tile, _cb, LV_EVENT_CLICKED, _UI);
  lv_obj_set_layout(tile, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(tile, 18, 0);
  lv_obj_set_style_pad_row(tile, 10, 0);

  lv_obj_t *title = lv_label_create(tile);
  lv_label_set_text(title, _title);
  lv_obj_set_style_text_color(title, lv_color_white(), 0);

  lv_obj_t *subtitle = lv_label_create(tile);
  lv_label_set_text(subtitle, _subtitle);
  lv_obj_set_style_text_color(subtitle, lv_color_hex(0xE5E5E5), 0);

  return tile;
}

static lv_obj_t *ui_create_input_row(UI *_UI, lv_obj_t *_parent,
                                     const char *_label,
                                     const char *_placeholder,
                                     lv_obj_t **_ta_out, bool _password) {
  lv_obj_t *wrap = lv_obj_create(_parent);
  lv_obj_set_width(wrap, LV_PCT(100));
  lv_obj_set_height(wrap, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(wrap, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(wrap, 0, 0);
  lv_obj_set_style_pad_all(wrap, 0, 0);
  lv_obj_set_layout(wrap, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(wrap, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(wrap, 6, 0);

  lv_obj_t *lbl = lv_label_create(wrap);
  lv_label_set_text(lbl, _label);
  lv_obj_set_style_text_color(lbl, lv_color_hex(0xD4D4D4), 0);

  lv_obj_t *ta = lv_textarea_create(wrap);
  lv_obj_set_width(ta, LV_PCT(100));
  lv_obj_set_height(ta, 50);
  lv_textarea_set_one_line(ta, true);
  lv_textarea_set_placeholder_text(ta, _placeholder);
  lv_textarea_set_password_mode(ta, _password);
  ui_apply_input_style(ta);
  lv_obj_add_event_cb(ta, textarea_changed_event_cb, LV_EVENT_VALUE_CHANGED,
                      _UI);
  lv_obj_add_event_cb(ta, textarea_focus_event_cb, LV_EVENT_FOCUSED, _UI);
  lv_obj_add_event_cb(ta, textarea_focus_event_cb, LV_EVENT_CLICKED, _UI);

  *_ta_out = ta;
  return wrap;
}

static void ui_hide_keyboard(UI *_UI) {
  if (!_UI || !_UI->keyboard)
    return;

  lv_keyboard_set_textarea(_UI->keyboard, NULL);
  lv_obj_add_flag(_UI->keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void ui_show_keyboard_for_textarea(UI *_UI, lv_obj_t *_ta) {
  if (!_UI || !_UI->keyboard || !_ta)
    return;

  lv_keyboard_set_textarea(_UI->keyboard, _ta);
  lv_obj_clear_flag(_UI->keyboard, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(_UI->keyboard);
}

static void keyboard_event_cb(lv_event_t *_event) {
  UI *ui = lv_event_get_user_data(_event);
  lv_event_code_t code = lv_event_get_code(_event);

  if (!ui)
    return;

  if (code == LV_EVENT_CANCEL || code == LV_EVENT_READY) {
    ui_hide_keyboard(ui);
  }
}

static void textarea_focus_event_cb(lv_event_t *_event) {
  UI *ui = lv_event_get_user_data(_event);
  lv_obj_t *ta = lv_event_get_target(_event);
  lv_event_code_t code = lv_event_get_code(_event);

  if (!ui || !ta)
    return;

  if (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED) {
    ui_show_keyboard_for_textarea(ui, ta);
  }
}

void ui_init(UI *_UI) {
  if (!_UI)
    return;

  memset(_UI, 0, sizeof(UI));
  _UI->current_screen = UI_SCREEN_HOME;
  _UI->wifi_state = UI_WIFI_STATE_DISCONNECTED;
  _UI->wifi_rssi = -100;
  ui_safe_copy(_UI->wifi_status, sizeof(_UI->wifi_status), "Not connected");
  ui_safe_copy(_UI->ip_address, sizeof(_UI->ip_address), "N/A");
  ui_update_datetime_string(_UI);

  lv_obj_t *screen = lv_scr_act();
  lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  _UI->root = lv_obj_create(screen);
  lv_obj_set_size(_UI->root, LV_PCT(100), LV_PCT(100));
  lv_obj_center(_UI->root);
  lv_obj_set_style_radius(_UI->root, 0, 0);
  lv_obj_set_style_border_width(_UI->root, 0, 0);
  lv_obj_set_style_pad_all(_UI->root, 0, 0);
  lv_obj_set_style_bg_color(_UI->root, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(_UI->root, LV_OPA_COVER, 0);
  lv_obj_set_layout(_UI->root, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(_UI->root, LV_FLEX_FLOW_COLUMN);

  _UI->header = lv_obj_create(_UI->root);
  lv_obj_set_width(_UI->header, LV_PCT(100));
  lv_obj_set_height(_UI->header, 72);
  lv_obj_set_style_radius(_UI->header, 0, 0);
  lv_obj_set_style_border_width(_UI->header, 0, 0);
  lv_obj_set_style_bg_color(_UI->header, lv_color_hex(0x0A0A0A), 0);
  lv_obj_set_style_bg_opa(_UI->header, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_left(_UI->header, 16, 0);
  lv_obj_set_style_pad_right(_UI->header, 16, 0);
  lv_obj_set_style_pad_top(_UI->header, 10, 0);
  lv_obj_set_style_pad_bottom(_UI->header, 10, 0);

  _UI->body = lv_obj_create(_UI->root);
  lv_obj_set_width(_UI->body, LV_PCT(100));
  lv_obj_set_flex_grow(_UI->body, 1);
  lv_obj_set_style_radius(_UI->body, 0, 0);
  lv_obj_set_style_border_width(_UI->body, 0, 0);
  lv_obj_set_style_bg_color(_UI->body, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(_UI->body, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(_UI->body, 0, 0);

  _UI->footer = lv_obj_create(_UI->root);
  lv_obj_set_width(_UI->footer, LV_PCT(100));
  lv_obj_set_height(_UI->footer, 40);
  lv_obj_set_style_radius(_UI->footer, 0, 0);
  lv_obj_set_style_border_width(_UI->footer, 0, 0);
  lv_obj_set_style_bg_color(_UI->footer, lv_color_hex(0x0A0A0A), 0);
  lv_obj_set_style_bg_opa(_UI->footer, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_left(_UI->footer, 12, 0);
  lv_obj_set_style_pad_right(_UI->footer, 12, 0);

  _UI->keyboard = lv_keyboard_create(screen);
  lv_obj_set_width(_UI->keyboard, LV_PCT(100));
  lv_obj_set_height(_UI->keyboard, 220);
  lv_obj_align(_UI->keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_flag(_UI->keyboard, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(_UI->keyboard, keyboard_event_cb, LV_EVENT_ALL, _UI);

  ui_rebuild(_UI);
}

static void ui_clear_containers(UI *_UI) {
  lv_obj_clean(_UI->header);
  lv_obj_clean(_UI->body);
  lv_obj_clean(_UI->footer);

  _UI->header_home_btn = NULL;
  _UI->header_title = NULL;
  _UI->header_wifi_btn = NULL;
  _UI->footer_status_label = NULL;
  _UI->footer_datetime_label = NULL;
  _UI->wifi_status_box = NULL;
  _UI->ssid_ta = NULL;
  _UI->password_ta = NULL;
  _UI->password_toggle_btn = NULL;
  _UI->connect_btn = NULL;
}

static void ui_rebuild(UI *_UI) {
  ui_hide_keyboard(_UI);
  ui_clear_containers(_UI);
  ui_build_header(_UI);

  switch (_UI->current_screen) {
  case UI_SCREEN_HOME:
    ui_build_home(_UI);
    break;
  case UI_SCREEN_SETTINGS:
    ui_build_settings(_UI);
    break;
  case UI_SCREEN_WIFI_CONFIG:
    ui_build_wifi_config(_UI);
    break;
  case UI_SCREEN_FACILITY_CONFIG:
    ui_build_facility_config(_UI);
    break;
  case UI_SCREEN_DEVICE_INFO:
    ui_build_device_info(_UI);
    break;
  case UI_SCREEN_FACTS:
    ui_build_facts(_UI);
    break;
  default:
    ui_build_home(_UI);
    break;
  }

  ui_build_footer(_UI);
}

static void ui_build_header(UI *_UI) {
  lv_obj_set_layout(_UI->header, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(_UI->header, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(_UI->header, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  _UI->header_home_btn = ui_create_button(_UI->header, LV_SYMBOL_HOME,
                                          lv_color_hex(0x262626), 48, 48);
  lv_obj_add_event_cb(_UI->header_home_btn, home_btn_event_cb, LV_EVENT_CLICKED,
                      _UI);

  _UI->header_title = lv_label_create(_UI->header);
  lv_label_set_text(_UI->header_title, "ESPMaestro");
  lv_obj_set_style_text_color(_UI->header_title, lv_color_white(), 0);

  _UI->header_wifi_btn =
      ui_create_button(_UI->header, "", lv_color_hex(0x262626), 72, 48);
  ui_update_wifi_header_indicator(_UI);
}

static void ui_update_wifi_header_indicator(UI *_UI) {
  if (!_UI || !_UI->header_wifi_btn)
    return;

  lv_obj_clean(_UI->header_wifi_btn);

  lv_color_t bars_color = lv_color_hex(0x737373);

  if (_UI->wifi_state == UI_WIFI_STATE_CONNECTING) {
    bars_color = lv_color_hex(0xEAB308);
    lv_obj_set_style_bg_color(_UI->header_wifi_btn, lv_color_hex(0x3A2F05), 0);
  } else if (_UI->wifi_state == UI_WIFI_STATE_CONNECTED) {
    bars_color = lv_color_hex(0x22C55E);
    lv_obj_set_style_bg_color(_UI->header_wifi_btn, lv_color_hex(0x052E16), 0);
  } else {
    bars_color = lv_color_hex(0xA3A3A3);
    lv_obj_set_style_bg_color(_UI->header_wifi_btn, lv_color_hex(0x262626), 0);
  }

  int bars = (_UI->wifi_state == UI_WIFI_STATE_CONNECTED)
                 ? ui_rssi_to_bars(_UI->wifi_rssi)
                 : 0;

  lv_obj_t *cont = lv_obj_create(_UI->header_wifi_btn);
  lv_obj_set_size(cont, 44, 24);
  lv_obj_center(cont);
  lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(cont, 0, 0);
  lv_obj_set_style_pad_all(cont, 0, 0);

  const lv_coord_t x[4] = {4, 14, 24, 34};
  const lv_coord_t h[4] = {6, 10, 14, 18};

  for (int i = 0; i < 4; i++) {
    lv_obj_t *bar = lv_obj_create(cont);
    lv_obj_set_size(bar, 6, h[i]);
    lv_obj_set_pos(bar, x[i], 22 - h[i]);
    lv_obj_set_style_radius(bar, 2, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);

    if (_UI->wifi_state == UI_WIFI_STATE_CONNECTED && i < bars)
      lv_obj_set_style_bg_color(bar, bars_color, 0);
    else if (_UI->wifi_state == UI_WIFI_STATE_CONNECTING)
      lv_obj_set_style_bg_color(bar, bars_color, 0);
    else
      lv_obj_set_style_bg_color(bar, lv_color_hex(0x737373), 0);
  }

  if (_UI->wifi_state == UI_WIFI_STATE_DISCONNECTED) {
    lv_obj_t *line = lv_line_create(cont);
    static lv_point_t pts[] = {{2, 20}, {42, 2}};
    lv_line_set_points(line, pts, 2);
    lv_obj_set_style_line_color(line, lv_color_hex(0xDC2626), 0);
    lv_obj_set_style_line_width(line, 3, 0);
  }
}

static void ui_build_footer(UI *_UI) {
  lv_obj_set_layout(_UI->footer, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(_UI->footer, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(_UI->footer, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  _UI->footer_status_label = lv_label_create(_UI->footer);
  _UI->footer_datetime_label = lv_label_create(_UI->footer);

  ui_update_footer_status(_UI);
  ui_update_footer_datetime(_UI);
}

static void ui_update_footer_status(UI *_UI) {
  if (!_UI || !_UI->footer_status_label)
    return;

  UBaseType_t stack_words = uxTaskGetStackHighWaterMark(NULL);
  unsigned stack_bytes = (unsigned)(stack_words * sizeof(StackType_t));

  lv_label_set_text_fmt(_UI->footer_status_label,
                        "Stack: %u words (~%u bytes free) [%s]",
                        (unsigned)stack_words, stack_bytes,
                        ui_stack_status_for_watermark(stack_words));

  lv_obj_set_style_text_color(_UI->footer_status_label,
                              ui_stack_color_for_watermark(stack_words), 0);
}

static void ui_update_footer_datetime(UI *_UI) {
  if (_UI && _UI->footer_datetime_label) {
    lv_label_set_text(_UI->footer_datetime_label, _UI->datetime_string);
    lv_obj_set_style_text_color(_UI->footer_datetime_label,
                                lv_color_hex(0xA3A3A3), 0);
  }
}

static void ui_build_home(UI *_UI) {
  lv_obj_set_style_pad_all(_UI->body, 24, 0);

  lv_obj_t *row = lv_obj_create(_UI->body);
  lv_obj_set_size(row, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_layout(row, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, 28, 0);

  ui_create_menu_tile(row, "Settings", "Configure device settings",
                      lv_color_hex(0x2563EB), home_settings_event_cb, _UI);

  ui_create_menu_tile(row, "Facts", "Open facts screen", lv_color_hex(0x9333EA),
                      home_facts_event_cb, _UI);
}

static void ui_build_settings(UI *_UI) {
  lv_obj_set_style_pad_all(_UI->body, 20, 0);

  lv_obj_t *panel = ui_create_card(_UI->body, 780, 430);
  lv_obj_center(panel);
  lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(panel, 18, 0);
  lv_obj_set_style_pad_row(panel, 14, 0);

  ui_create_menu_tile(panel, "WiFi Configuration", "SSID / password / connect",
                      lv_color_hex(0x2563EB), settings_wifi_event_cb, _UI);
  ui_create_menu_tile(panel, "Facility Configuration", "Facility form",
                      lv_color_hex(0x16A34A), settings_facility_event_cb, _UI);
  ui_create_menu_tile(panel, "Device Info", "Current device information screen",
                      lv_color_hex(0x525252), settings_device_info_event_cb,
                      _UI);
}

static void ui_update_wifi_status_box(UI *_UI) {
  if (!_UI || !_UI->wifi_status_box)
    return;

  lv_obj_clean(_UI->wifi_status_box);

  lv_color_t bg = lv_color_hex(0x262626);
  lv_color_t border = lv_color_hex(0x404040);
  lv_color_t text = lv_color_hex(0xA3A3A3);

  if (_UI->wifi_state == UI_WIFI_STATE_CONNECTING) {
    bg = lv_color_hex(0x3A2F05);
    border = lv_color_hex(0xEAB308);
    text = lv_color_hex(0xFDE68A);
  } else if (_UI->wifi_state == UI_WIFI_STATE_CONNECTED) {
    bg = lv_color_hex(0x052E16);
    border = lv_color_hex(0x16A34A);
    text = lv_color_hex(0x86EFAC);
  } else {
    bg = lv_color_hex(0x2A2A2A);
    border = lv_color_hex(0x525252);
    text = lv_color_hex(0xD4D4D4);
  }

  lv_obj_set_style_bg_color(_UI->wifi_status_box, bg, 0);
  lv_obj_set_style_border_color(_UI->wifi_status_box, border, 0);
  lv_obj_set_style_border_width(_UI->wifi_status_box, 1, 0);
  lv_obj_set_style_radius(_UI->wifi_status_box, 12, 0);

  lv_obj_t *lbl = lv_label_create(_UI->wifi_status_box);
  lv_label_set_text(lbl, _UI->wifi_status);
  lv_obj_set_style_text_color(lbl, text, 0);
  lv_obj_center(lbl);
}

static void ui_update_connect_button(UI *_UI) {
  if (!_UI || !_UI->connect_btn)
    return;

  bool disable =
      (strlen(_UI->pending_ssid) == 0 || strlen(_UI->pending_password) == 0 ||
       _UI->wifi_state == UI_WIFI_STATE_CONNECTING);

  if (disable)
    lv_obj_add_state(_UI->connect_btn, LV_STATE_DISABLED);
  else
    lv_obj_clear_state(_UI->connect_btn, LV_STATE_DISABLED);
}

static void ui_build_wifi_config(UI *_UI) {
  lv_obj_set_style_pad_all(_UI->body, 20, 0);

  lv_obj_t *panel = ui_create_card(_UI->body, 780, 460);
  lv_obj_center(panel);
  lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(panel, 18, 0);
  lv_obj_set_style_pad_row(panel, 12, 0);

  ui_create_input_row(_UI, panel, "Network Name (SSID)",
                      "Enter WiFi network name", &_UI->ssid_ta, false);
  lv_textarea_set_text(_UI->ssid_ta, _UI->pending_ssid);

  lv_obj_t *pwd_wrap = lv_obj_create(panel);
  lv_obj_set_width(pwd_wrap, LV_PCT(100));
  lv_obj_set_height(pwd_wrap, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(pwd_wrap, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(pwd_wrap, 0, 0);
  lv_obj_set_style_pad_all(pwd_wrap, 0, 0);
  lv_obj_set_layout(pwd_wrap, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(pwd_wrap, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(pwd_wrap, 6, 0);

  lv_obj_t *pwd_lbl = lv_label_create(pwd_wrap);
  lv_label_set_text(pwd_lbl, "Password");
  lv_obj_set_style_text_color(pwd_lbl, lv_color_hex(0xD4D4D4), 0);

  lv_obj_t *pwd_row = lv_obj_create(pwd_wrap);
  lv_obj_set_width(pwd_row, LV_PCT(100));
  lv_obj_set_height(pwd_row, 50);
  lv_obj_set_style_bg_opa(pwd_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(pwd_row, 0, 0);
  lv_obj_set_style_pad_all(pwd_row, 0, 0);

  _UI->password_ta = lv_textarea_create(pwd_row);
  lv_obj_set_width(_UI->password_ta, LV_PCT(100));
  lv_obj_set_height(_UI->password_ta, 50);
  lv_textarea_set_one_line(_UI->password_ta, true);
  lv_textarea_set_password_mode(_UI->password_ta, !_UI->show_password);
  lv_textarea_set_placeholder_text(_UI->password_ta, "Enter WiFi password");
  lv_textarea_set_text(_UI->password_ta, _UI->pending_password);
  ui_apply_input_style(_UI->password_ta);
  lv_obj_add_event_cb(_UI->password_ta, textarea_changed_event_cb,
                      LV_EVENT_VALUE_CHANGED, _UI);
  lv_obj_add_event_cb(_UI->password_ta, textarea_focus_event_cb,
                      LV_EVENT_FOCUSED, _UI);
  lv_obj_add_event_cb(_UI->password_ta, textarea_focus_event_cb,
                      LV_EVENT_CLICKED, _UI);

  _UI->password_toggle_btn =
      ui_create_button(pwd_row, _UI->show_password ? "Hide" : "Show",
                       lv_color_hex(0x262626), 64, 38);
  lv_obj_align(_UI->password_toggle_btn, LV_ALIGN_RIGHT_MID, -6, 0);
  lv_obj_add_event_cb(_UI->password_toggle_btn, password_toggle_event_cb,
                      LV_EVENT_CLICKED, _UI);

  _UI->wifi_status_box = ui_create_card(panel, LV_PCT(100), 56);
  ui_update_wifi_status_box(_UI);

  _UI->connect_btn = ui_create_button(panel, LV_SYMBOL_WIFI " Connect to WiFi",
                                      lv_color_hex(0x2563EB), 740, 50);
  lv_obj_add_event_cb(_UI->connect_btn, connect_event_cb, LV_EVENT_CLICKED,
                      _UI);
  ui_update_connect_button(_UI);
}

static void ui_build_facility_config(UI *_UI) {
  lv_obj_set_style_pad_all(_UI->body, 20, 0);

  lv_obj_t *panel = ui_create_card(_UI->body, 780, 460);
  lv_obj_center(panel);
  lv_obj_add_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(panel, 18, 0);
  lv_obj_set_style_pad_row(panel, 10, 0);

  ui_create_input_row(_UI, panel, "Facility Name", "Enter facility name",
                      &_UI->facility_name_ta, false);
  ui_create_input_row(_UI, panel, "Country", "Enter country", &_UI->country_ta,
                      false);
  ui_create_input_row(_UI, panel, "Address", "Enter address", &_UI->address_ta,
                      false);
  ui_create_input_row(_UI, panel, "City", "Enter city", &_UI->city_ta, false);
  ui_create_input_row(_UI, panel, "Postal Code", "Enter postal code",
                      &_UI->postal_code_ta, false);
  ui_create_input_row(_UI, panel, "Currency", "Enter currency",
                      &_UI->currency_ta, false);
  ui_create_input_row(_UI, panel, "Energy Zone", "Enter energy zone",
                      &_UI->energy_zone_ta, false);
  ui_create_input_row(_UI, panel, "Solar Panel Tilt Degree",
                      "Enter solar panel tilt degree", &_UI->solar_tilt_ta,
                      false);
  ui_create_input_row(_UI, panel, "Solar Panel Azimuth",
                      "Enter solar panel azimuth", &_UI->solar_azimuth_ta,
                      false);
  ui_create_input_row(_UI, panel, "Solar Panel Size", "Enter solar panel size",
                      &_UI->solar_size_ta, false);

  ui_create_button(panel, LV_SYMBOL_SAVE " Save Settings",
                   lv_color_hex(0x2563EB), 740, 48);
}

static void ui_build_device_info(UI *_UI) {
  lv_obj_set_style_pad_all(_UI->body, 20, 0);

  lv_obj_t *panel = ui_create_card(_UI->body, 780, 420);
  lv_obj_center(panel);
  lv_obj_add_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(panel, 18, 0);
  lv_obj_set_style_pad_row(panel, 10, 0);

  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);

  char mem_info[192];
  snprintf(mem_info, sizeof(mem_info),
           "Total heap: %.2f kB\n"
           "Internal heap: %.2f kB\n"
           "External heap: %.2f kB\n"
           "Largest free block: %u bytes",
           (float)esp_get_free_heap_size() / 1024.0f,
           (float)heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024.0f,
           (float)heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024.0f,
           heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

  char chip_info_str[192];
  snprintf(chip_info_str, sizeof(chip_info_str),
           "Cores: %d\n"
           "Revision: %d\n"
           "Features: %s%s%s%s",
           chip_info.cores, chip_info.revision,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi " : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE " : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT " : "",
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "Embedded-Flash"
                                                         : "");

  UBaseType_t stack_words = uxTaskGetStackHighWaterMark(NULL);
  unsigned stack_bytes = (unsigned)(stack_words * sizeof(StackType_t));

  char stack_info[160];
  snprintf(stack_info, sizeof(stack_info),
           "Current task stack high watermark: %u words (~%u bytes free)\n"
           "Status: %s",
           (unsigned)stack_words, stack_bytes,
           ui_stack_status_for_watermark(stack_words));

  lv_obj_t *title = lv_label_create(panel);
  lv_label_set_text(title, "Device Info");
  lv_obj_set_style_text_color(title, lv_color_white(), 0);

  lv_obj_t *ip = lv_label_create(panel);
  lv_label_set_text_fmt(ip, "IP Address: %s", _UI->ip_address);
  lv_obj_set_style_text_color(ip, lv_color_hex(0xD4D4D4), 0);

  lv_obj_t *ssid = lv_label_create(panel);
  lv_label_set_text_fmt(ssid, "SSID: %s",
                        _UI->connected_ssid[0] ? _UI->connected_ssid : "N/A");
  lv_obj_set_style_text_color(ssid, lv_color_hex(0xD4D4D4), 0);

  lv_obj_t *wifi = lv_label_create(panel);
  lv_label_set_text_fmt(wifi, "WiFi State: %s",
                        _UI->wifi_state == UI_WIFI_STATE_CONNECTED
                            ? "Connected"
                            : (_UI->wifi_state == UI_WIFI_STATE_CONNECTING
                                   ? "Connecting"
                                   : "Disconnected"));
  lv_obj_set_style_text_color(wifi, lv_color_hex(0xD4D4D4), 0);

  lv_obj_t *rssi = lv_label_create(panel);
  lv_label_set_text_fmt(rssi, "RSSI: %d dBm", _UI->wifi_rssi);
  lv_obj_set_style_text_color(rssi, lv_color_hex(0xD4D4D4), 0);

  lv_obj_t *time_lbl = lv_label_create(panel);
  lv_label_set_text_fmt(time_lbl, "Time: %s", _UI->datetime_string);
  lv_obj_set_style_text_color(time_lbl, lv_color_hex(0xD4D4D4), 0);

  lv_obj_t *chip_lbl = lv_label_create(panel);
  lv_label_set_text_fmt(chip_lbl, "Chip Info:\n%s", chip_info_str);
  lv_obj_set_style_text_color(chip_lbl, lv_color_hex(0xD4D4D4), 0);

  lv_obj_t *mem_lbl = lv_label_create(panel);
  lv_label_set_text_fmt(mem_lbl, "Memory Info:\n%s", mem_info);
  lv_obj_set_style_text_color(mem_lbl, lv_color_hex(0xD4D4D4), 0);

  lv_obj_t *stack_lbl = lv_label_create(panel);
  lv_label_set_text_fmt(stack_lbl, "Stack Info:\n%s", stack_info);
  lv_obj_set_style_text_color(stack_lbl,
                              ui_stack_color_for_watermark(stack_words), 0);
}

static void ui_build_facts(UI *_UI) {
  lv_obj_set_style_pad_all(_UI->body, 20, 0);

  lv_obj_t *panel = ui_create_card(_UI->body, 780, 420);
  lv_obj_center(panel);
  lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(panel, 18, 0);
  lv_obj_set_style_pad_row(panel, 10, 0);

  lv_obj_t *title = lv_label_create(panel);
  lv_label_set_text(title, "Facts");
  lv_obj_set_style_text_color(title, lv_color_white(), 0);

  lv_obj_t *txt = lv_label_create(panel);
  lv_label_set_text(txt, "Placeholder for the Figma Facts screen.");
  lv_obj_set_style_text_color(txt, lv_color_hex(0xD4D4D4), 0);
}

void ui_show_home_screen(UI *_UI) {
  if (!_UI)
    return;
  _UI->current_screen = UI_SCREEN_HOME;
  ui_rebuild(_UI);
}

void ui_set_wifi_connecting(UI *_UI) {
  if (!_UI)
    return;

  _UI->wifi_state = UI_WIFI_STATE_CONNECTING;
  _UI->wifi_connected = false;
  ui_safe_copy(_UI->wifi_status, sizeof(_UI->wifi_status),
               "Connecting to network...");
  ui_update_wifi_header_indicator(_UI);
  ui_update_wifi_status_box(_UI);
  ui_update_connect_button(_UI);
}

void ui_set_wifi_failed(UI *_UI, const char *_reason) {
  if (!_UI)
    return;

  _UI->wifi_state = UI_WIFI_STATE_DISCONNECTED;
  _UI->wifi_connected = false;
  _UI->wifi_rssi = -100;
  ui_safe_copy(_UI->wifi_status, sizeof(_UI->wifi_status),
               _reason ? _reason : "Connection failed");
  ui_safe_copy(_UI->connected_ssid, sizeof(_UI->connected_ssid), "");
  ui_safe_copy(_UI->ip_address, sizeof(_UI->ip_address), "N/A");

  ui_update_wifi_header_indicator(_UI);
  ui_update_wifi_status_box(_UI);
  ui_update_connect_button(_UI);
}

void ui_set_wifi_status(UI *_UI, bool _connected, const char *_ssid,
                        const char *_ip, int _rssi) {
  if (!_UI)
    return;

  _UI->wifi_connected = _connected;
  _UI->wifi_rssi = _rssi;

  if (_connected) {
    _UI->wifi_state = UI_WIFI_STATE_CONNECTED;
    ui_safe_copy(_UI->wifi_status, sizeof(_UI->wifi_status),
                 "Successfully connected");
    ui_safe_copy(_UI->connected_ssid, sizeof(_UI->connected_ssid),
                 _ssid ? _ssid : "");
    ui_safe_copy(_UI->ip_address, sizeof(_UI->ip_address), _ip ? _ip : "N/A");
  } else {
    _UI->wifi_state = UI_WIFI_STATE_DISCONNECTED;
    ui_safe_copy(_UI->wifi_status, sizeof(_UI->wifi_status), "Not connected");
    ui_safe_copy(_UI->connected_ssid, sizeof(_UI->connected_ssid), "");
    ui_safe_copy(_UI->ip_address, sizeof(_UI->ip_address), "N/A");
  }

  ui_update_wifi_header_indicator(_UI);
  ui_update_wifi_status_box(_UI);
}

bool ui_consume_wifi_connect_request(UI *_UI, char *_ssid, unsigned _ssid_sz,
                                     char *_password, unsigned _password_sz) {
  if (!_UI || !_UI->wifi_connect_request_pending)
    return false;

  _UI->wifi_connect_request_pending = false;

  if (_ssid && _ssid_sz > 0)
    snprintf(_ssid, _ssid_sz, "%s", _UI->pending_ssid);
  if (_password && _password_sz > 0)
    snprintf(_password, _password_sz, "%s", _UI->pending_password);

  return true;
}

void ui_tick(UI *_UI) {
  if (!_UI)
    return;
  ui_update_datetime_string(_UI);
  ui_update_footer_status(_UI);
  ui_update_footer_datetime(_UI);
}

static void home_btn_event_cb(lv_event_t *_event) {
  UI *ui = lv_event_get_user_data(_event);
  ui->current_screen = UI_SCREEN_HOME;
  ui_rebuild(ui);
}

static void home_settings_event_cb(lv_event_t *_event) {
  UI *ui = lv_event_get_user_data(_event);
  ui->current_screen = UI_SCREEN_SETTINGS;
  ui_rebuild(ui);
}

static void home_facts_event_cb(lv_event_t *_event) {
  UI *ui = lv_event_get_user_data(_event);
  ui->current_screen = UI_SCREEN_FACTS;
  ui_rebuild(ui);
}

static void settings_wifi_event_cb(lv_event_t *_event) {
  UI *ui = lv_event_get_user_data(_event);
  ui->current_screen = UI_SCREEN_WIFI_CONFIG;
  ui_rebuild(ui);
}

static void settings_facility_event_cb(lv_event_t *_event) {
  UI *ui = lv_event_get_user_data(_event);
  ui->current_screen = UI_SCREEN_FACILITY_CONFIG;
  ui_rebuild(ui);
}

static void settings_device_info_event_cb(lv_event_t *_event) {
  UI *ui = lv_event_get_user_data(_event);
  ui->current_screen = UI_SCREEN_DEVICE_INFO;
  ui_rebuild(ui);
}

static void password_toggle_event_cb(lv_event_t *_event) {
  UI *ui = lv_event_get_user_data(_event);
  ui->show_password = !ui->show_password;

  if (ui->password_ta)
    lv_textarea_set_password_mode(ui->password_ta, !ui->show_password);

  if (ui->password_toggle_btn) {
    lv_obj_t *lbl = lv_obj_get_child(ui->password_toggle_btn, 0);
    if (lbl)
      lv_label_set_text(lbl, ui->show_password ? "Hide" : "Show");
  }
}

static void textarea_changed_event_cb(lv_event_t *_event) {
  UI *ui = lv_event_get_user_data(_event);
  lv_obj_t *ta = lv_event_get_target(_event);
  const char *txt = lv_textarea_get_text(ta);

  if (ta == ui->ssid_ta)
    ui_safe_copy(ui->pending_ssid, sizeof(ui->pending_ssid), txt);
  else if (ta == ui->password_ta)
    ui_safe_copy(ui->pending_password, sizeof(ui->pending_password), txt);
  else if (ta == ui->facility_name_ta)
    ui_safe_copy(ui->facility_name, sizeof(ui->facility_name), txt);
  else if (ta == ui->country_ta)
    ui_safe_copy(ui->country, sizeof(ui->country), txt);
  else if (ta == ui->address_ta)
    ui_safe_copy(ui->address, sizeof(ui->address), txt);
  else if (ta == ui->city_ta)
    ui_safe_copy(ui->city, sizeof(ui->city), txt);
  else if (ta == ui->postal_code_ta)
    ui_safe_copy(ui->postal_code, sizeof(ui->postal_code), txt);
  else if (ta == ui->currency_ta)
    ui_safe_copy(ui->currency, sizeof(ui->currency), txt);
  else if (ta == ui->energy_zone_ta)
    ui_safe_copy(ui->energy_zone, sizeof(ui->energy_zone), txt);
  else if (ta == ui->solar_tilt_ta)
    ui_safe_copy(ui->solar_tilt, sizeof(ui->solar_tilt), txt);
  else if (ta == ui->solar_azimuth_ta)
    ui_safe_copy(ui->solar_azimuth, sizeof(ui->solar_azimuth), txt);
  else if (ta == ui->solar_size_ta)
    ui_safe_copy(ui->solar_size, sizeof(ui->solar_size), txt);

  ui_update_connect_button(ui);
}

static void connect_event_cb(lv_event_t *_event) {
  UI *ui = lv_event_get_user_data(_event);

  if (strlen(ui->pending_ssid) == 0 || strlen(ui->pending_password) == 0)
    return;

  ui->wifi_connect_request_pending = true;
  ui_set_wifi_connecting(ui);

  ESP_LOGI(TAG, "WiFi connect requested from UI for SSID: %s",
           ui->pending_ssid);
}
