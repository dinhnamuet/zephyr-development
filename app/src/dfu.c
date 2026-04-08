#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/dfu/flash_img.h>
#include <zephyr/sys/reboot.h>

#include "usb.h"
#include "dfu.h"
#include "tcp_server.h"

LOG_MODULE_REGISTER(dfu_func);

#define DFU_START    0x03
#define DFU_DOWNLOAD 0x04
#define DFU_END      0x05
#define NUM_RQ       (DFU_END - DFU_START + 1)

#define SUCCESS      1
#define FAILED       0

static struct flash_img_context dfu_ctx;

#if defined(DFU_NET_TCP)
static void dfu_start(int fd, const uint8_t *buf, size_t size)
#else
static void dfu_start(const uint8_t *buf, size_t size)
#endif
{
    uint8_t feedback[2] = { DFU_START, SUCCESS };

    if (flash_img_init(&dfu_ctx)) {
        LOG_ERR("Couldnot init flash for DFU");
        feedback[1] = FAILED;
    }
#if defined(DFU_NET_TCP)
        tcp_server_send(fd, feedback, 2);
#else
        usb_app_send(feedback, 2);
#endif
}
#if defined(DFU_NET_TCP)
static void dfu_download(int fd, const uint8_t *buf, size_t size)
#else
static void dfu_download(const uint8_t *buf, size_t size)
#endif
{
    uint8_t feedback[2] = { DFU_DOWNLOAD, SUCCESS };

    if (flash_img_buffered_write(&dfu_ctx, buf, size, false)) {
        LOG_ERR("Write failed");
        feedback[1] = FAILED;
    }
#if defined(DFU_NET_TCP)
        tcp_server_send(fd, feedback, 2);
#else
        usb_app_send(feedback, 2);
#endif
}

#if defined(DFU_NET_TCP)
static void dfu_end(int fd, const uint8_t *buf, size_t size)
#else
static void dfu_end(const uint8_t *buf, size_t size)
#endif
{
    uint8_t feedback[2] = { DFU_END, SUCCESS };
    
    if (flash_img_buffered_write(&dfu_ctx, NULL, 0, true)) {
        LOG_ERR("Write failed");
        feedback[1] = FAILED;
#if defined(DFU_NET_TCP)
        tcp_server_send(fd, feedback, 2);
#else
        usb_app_send(feedback, 2);
#endif
        return;
    }
#if defined(DFU_NET_TCP)
    tcp_server_send(fd, feedback, 2);
#else
    usb_app_send(feedback, 2);
#endif
    boot_request_upgrade(BOOT_UPGRADE_TEST);
#if !defined(DFU_NET_TCP)
    usb_app_disable();
#endif
    sys_reboot(SYS_REBOOT_COLD);
}

#if defined(DFU_NET_TCP)
static void (*lookup_table[NUM_RQ])(int fd, const uint8_t *, size_t) = {
#else
static void (*lookup_table[NUM_RQ])(const uint8_t *, size_t) = {
#endif
    dfu_start, dfu_download, dfu_end
};

#if defined(DFU_NET_TCP)
void dfu_handle(int fd, uint8_t req, const uint8_t *buf, size_t size)
#else
void dfu_handle(uint8_t req, const uint8_t *buf, size_t size)
#endif
{
    if (req < DFU_START || req > DFU_END) {
        uint8_t feedback[2] = { req, FAILED };
#if defined(DFU_NET_TCP)
        tcp_server_send(fd, feedback, 2);
#else
        usb_app_send(feedback, 2);
#endif
        return;
    }
#if defined(DFU_NET_TCP)
    lookup_table[req - DFU_START](fd, buf, size);
#else
    lookup_table[req - DFU_START](buf, size);
#endif
}
