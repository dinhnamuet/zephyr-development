#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/dfu/flash_img.h>
#include <zephyr/sys/reboot.h>

#include "usb.h"
#include "dfu.h"

LOG_MODULE_REGISTER(dfu_func);

#define DFU_START    0x03
#define DFU_DOWNLOAD 0x04
#define DFU_END      0x05
#define NUM_RQ       (DFU_END - DFU_START + 1)

#define SUCCESS      1
#define FAILED       0

static struct flash_img_context dfu_ctx;

static void dfu_start(const uint8_t *buf, size_t size)
{
    uint8_t feedback[2] = { DFU_START, SUCCESS };

    if (flash_img_init(&dfu_ctx)) {
        LOG_ERR("Couldnot init flash for DFU");
        feedback[1] = FAILED;
    }
    usb_app_send(feedback, 2);
}

static void dfu_download(const uint8_t *buf, size_t size)
{
    uint8_t feedback[2] = { DFU_DOWNLOAD, SUCCESS };

    if (flash_img_buffered_write(&dfu_ctx, buf, size, false)) {
        LOG_ERR("Write failed");
        feedback[1] = FAILED;
    }
    usb_app_send(feedback, 2);
}

static void dfu_end(const uint8_t *buf, size_t size)
{
    uint8_t feedback[2] = { DFU_END, SUCCESS };
    
    if (flash_img_buffered_write(&dfu_ctx, NULL, 0, true)) {
        LOG_ERR("Write failed");
        feedback[1] = FAILED;
        usb_app_send(feedback, 2);
        return;
    }
    usb_app_send(feedback, 2);
    boot_request_upgrade(BOOT_UPGRADE_TEST);
    usb_app_disable();
    sys_reboot(SYS_REBOOT_COLD);
}

static void (*lookup_table[NUM_RQ])(const uint8_t *, size_t) = {
    dfu_start, dfu_download, dfu_end
};

void dfu_handle(uint8_t req, const uint8_t *buf, size_t size)
{
    if (req < DFU_START || req > DFU_END) {
        uint8_t feedback[2] = { req, FAILED };
        usb_app_send(feedback, 2);
        return;
    }
    
    lookup_table[req - DFU_START](buf, size);
}
