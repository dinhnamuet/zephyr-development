#include <zephyr/kernel.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include "usb.h"
#include "custom_class.h"

LOG_MODULE_REGISTER(usb_app);

#define USB_VID              0x0208
#define USB_PID              0x2002
#define USB_MFR_STR          "DinhNamUET"
#define USB_PRODUCT_STR      "STM32H743ZI Zephyr"
#define USB_CFG_STR          "FullSpeed Configuration"
#define USB_MAX_POWER_MA     100

USBD_DEVICE_DEFINE(usb_dev,
    DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
    USB_VID,
    USB_PID);

USBD_DESC_LANG_DEFINE(usb_lang);
USBD_DESC_MANUFACTURER_DEFINE(usb_mfr, USB_MFR_STR);
USBD_DESC_PRODUCT_DEFINE(usb_product, USB_PRODUCT_STR);
USBD_DESC_CONFIG_DEFINE(usb_cfg_desc, USB_CFG_STR);
USBD_CONFIGURATION_DEFINE(usb_fs_cfg, USB_SCD_SELF_POWERED, (USB_MAX_POWER_MA / 2), &usb_cfg_desc);

#define MAX_NUM_RX            5
#define USB_FS_MTU            64U
#define USB_RX_THREAD_STACK   512
#define USB_RX_THREAD_PRIO    5

struct usb_rx_frame {
    void   *fifo_reserved;
    size_t  size;
    uint8_t data[];
};

K_THREAD_STACK_DEFINE(usb_rx_stack, USB_RX_THREAD_STACK);
K_HEAP_DEFINE(usb_heap, MAX_NUM_RX * (sizeof(struct usb_rx_frame) + USB_FS_MTU));
K_FIFO_DEFINE(rx_fifo);
static usb_rx_handler_t user_rx_handler;
static const struct device *custom_class_dev;
static atomic_t usb_ready = ATOMIC_INIT(0);
static struct k_thread usb_thread;

static void usb_msg_cb(struct usbd_context *const ctx,
                       const struct usbd_msg *const msg)
{
    LOG_INF("USB event: %s", usbd_msg_type_string(msg->type));

    switch (msg->type) {
    case USBD_MSG_CONFIGURATION:
        LOG_INF("Configured (value=%d)", msg->status);
        atomic_set(&usb_ready, 1);
        break;

    case USBD_MSG_SUSPEND:
        atomic_set(&usb_ready, 0);
        break;

    case USBD_MSG_RESUME:
        atomic_set(&usb_ready, 1);
        break;

    case USBD_MSG_VBUS_READY:
        if (usbd_enable(&usb_dev)) {
            LOG_ERR("usbd_enable failed");
        }
        break;

    case USBD_MSG_VBUS_REMOVED:
        atomic_set(&usb_ready, 0);
        usbd_disable(&usb_dev);
        break;

    default:
        break;
    }
}

static void usb_rx_cb(const uint8_t *data, size_t size)
{
    struct usb_rx_frame *frame;

    frame = k_heap_alloc(&usb_heap, sizeof(*frame) + size, K_NO_WAIT);
    if (!frame) {
        LOG_ERR("Out of memory");
        LOG_WRN("Drop USB message");
        return;
    }
    frame->size = size;
    memcpy(frame->data, data, size);

    k_fifo_put(&rx_fifo, frame);
}

static void usb_rx_thread(void *a, void *b, void *c)
{
    while (true) {
        struct usb_rx_frame *frame = k_fifo_get(&rx_fifo, K_FOREVER);

        if (user_rx_handler)
            user_rx_handler(frame->data, frame->size);

        k_heap_free(&usb_heap, frame);
    }
}

int usb_app_init(usb_rx_handler_t rx_handler)
{
    int ret;

    custom_class_dev = device_get_binding("custom_class");

    if (!device_is_ready(custom_class_dev)) {
        LOG_ERR("custom_class device not ready");
        return -ENODEV;
    }

    user_rx_handler = rx_handler;
    custom_class_set_rx_callback(custom_class_dev, usb_rx_cb);

    ret = usbd_msg_register_cb(&usb_dev, usb_msg_cb);
    if (ret) {
        LOG_ERR("usbd_msg_register_cb failed: %d", ret);
        return ret;
    }

    ret = usbd_add_descriptor(&usb_dev, &usb_lang);
    if (ret) { LOG_ERR("add lang desc: %d", ret); return ret; }

    ret = usbd_add_descriptor(&usb_dev, &usb_mfr);
    if (ret) { LOG_ERR("add mfr desc: %d", ret); return ret; }

    ret = usbd_add_descriptor(&usb_dev, &usb_product);
    if (ret) { LOG_ERR("add product desc: %d", ret); return ret; }

    ret = usbd_add_configuration(&usb_dev, USBD_SPEED_FS, &usb_fs_cfg);
    if (ret) { LOG_ERR("add fs config: %d", ret); return ret; }

    ret = usbd_register_class(&usb_dev, "custom_class_0", USBD_SPEED_FS, 1);
    if (ret) { LOG_ERR("register class: %d", ret); return ret; }

    ret = usbd_device_set_code_triple(&usb_dev, USBD_SPEED_FS, USB_BCC_VENDOR, \
    USB_BCC_VENDOR, USB_BCC_VENDOR);
    if (ret) { LOG_ERR("Set device class: %d", ret); return ret; }

    ret = usbd_init(&usb_dev);
    if (ret) { LOG_ERR("usbd_init: %d", ret); return ret; }

    if (!usbd_can_detect_vbus(&usb_dev)) {
        ret = usbd_enable(&usb_dev);
        if (ret) { LOG_ERR("usbd_enable: %d", ret); return ret; }
    }

    LOG_INF("USB initialized (VID=0x%04X PID=0x%04X)", USB_VID, USB_PID);
    k_thread_create(&usb_thread, usb_rx_stack,
				  K_THREAD_STACK_SIZEOF(usb_rx_stack),
				  usb_rx_thread,
				  NULL, NULL, NULL,
				  USB_RX_THREAD_PRIO, 0, K_NO_WAIT);
    return 0;
}

int usb_app_send(const uint8_t *buf, size_t size)
{
    if (!atomic_get(&usb_ready))
        return -ENOTCONN;
    else if (!buf && size)
        return -EINVAL;

    return custom_class_send(custom_class_dev, buf, size);
}

bool usb_app_is_ready(void)
{
    return (bool)atomic_get(&usb_ready);
}

void usb_app_disable(void)
{
    usbd_disable(&usb_dev);
    usbd_shutdown(&usb_dev);
}
