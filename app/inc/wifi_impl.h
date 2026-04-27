#pragma once

int wifi_init(void);
int wifi_connect(void);
int wifi_save(const char *ssid, const char *password);
int wifi_del(const char *ssid);
const char *wifi_get_ipv4(void);
