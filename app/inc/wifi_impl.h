#pragma once

int wifi_init(void);
int wifi_connect(const char *ssid, const char *password);
int wifi_save_default(const char *ssid, const char *password);
const char *wifi_get_ipv4(void);
