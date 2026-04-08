#pragma once

#include <sys/types.h>

#define STRING_ID        1
#define INTEGER_ID       2
#define BOOT_COUNT_ID    3
#define WIFI_SSID_ID     4
#define WIFI_PSK_ID      5

int storage_init(void);
ssize_t storage_read(uint16_t id, void *data, size_t len);
ssize_t storage_write(uint16_t id, const void *data, size_t len);
