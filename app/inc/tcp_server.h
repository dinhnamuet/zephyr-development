#pragma once

typedef void (*tcp_rx_t)(int client_fd, const uint8_t *buf, size_t size);

int tcp_server_init(uint16_t port);
int tcp_server_start(tcp_rx_t cb);
ssize_t tcp_server_send(int fd, const uint8_t *buf, size_t len);
