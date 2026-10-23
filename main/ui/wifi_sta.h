#pragma once
#include <stdbool.h>

void wifi_sta_init(void);
bool wifi_sta_is_connected(void);
const char *wifi_sta_ip(void);
const char *wifi_sta_dns(void);
int wifi_sta_rssi(void);
