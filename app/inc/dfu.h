#pragma once

/**
 * request: <1 byte cmd> + <data>
 * response: <1 byte cmd> + <1 byte result>
 * 0: failed
 * 1: successfully
 */
void dfu_handle(uint8_t req, const uint8_t *buf, size_t size);
