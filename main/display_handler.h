#ifndef __ESPM_DISPLAY_HANDLER_H__
#define __ESPM_DISPLAY_HANDLER_H__

#include <stdbool.h>

#define DISPLAY_SIZE_WIDTH 1024
#define DISPLAY_SIZE_HEIGHT 600
#define DISPLAY_MAX_CHAR_PER_ROW 73
#define DISPLAY_MAX_CHAR_ROWS 42

typedef struct {

} DH;

int display_handler_init(DH *_DH);
void display_handler_work(void *_null_for_now);

void display_handler_wifi_status(bool connected, const char *ssid,
                                 const char *ip, int rssi);
void display_handler_wifi_connecting(void);
void display_handler_wifi_failed(const char *reason);

bool display_handler_consume_wifi_connect_request(char *ssid, unsigned ssid_sz,
                                                  char *password,
                                                  unsigned password_sz);

#endif
