#pragma once
#define DFU_NET_TCP
/**
 * request: <1 byte cmd> + <data>
 * response: <1 byte cmd> + <1 byte result>
 * 0: failed
 * 1: successfully
 */
#if defined(DFU_NET_TCP)
void dfu_handle(int fd, uint8_t req, const uint8_t *buf, size_t size);
#else
void dfu_handle(uint8_t req, const uint8_t *buf, size_t size);
#endif
