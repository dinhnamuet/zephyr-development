#pragma once

typedef void (*uart_cb_t)(const uint8_t *data, size_t size);

int serial_init(uart_cb_t cb);
int serial_write(const uint8_t *data, size_t size);
