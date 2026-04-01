#pragma once

typedef void (*usb_rx_handler_t)(const uint8_t *data, size_t size);

int usb_app_init(usb_rx_handler_t rx_handler);
int usb_app_send(const uint8_t *buf, size_t size);
bool usb_app_is_ready(void);
void usb_app_disable(void);
