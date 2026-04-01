#include <zephyr/device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/drivers/usb/udc.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>

#include "custom_class.h"

LOG_MODULE_REGISTER(usbd_custom_class);

#define NUM_ENDPOINTS 2

struct custom_class_desc {
	struct usb_if_descriptor intf;
	struct usb_ep_descriptor ep1out;
	struct usb_ep_descriptor ep1in;
	struct usb_desc_header nil;
} __packed;

struct custom_class_data {
	struct usbd_class_data *class_node;
	struct custom_class_desc *const desc;
	const struct usb_desc_header **fs_desc;
	usb_rxcb_t rx_callback;
	struct k_mutex lock;
};

static int
custom_class_do_send(const struct device *dev, const uint8_t *buf, size_t len)
{
	int ret;
	struct net_buf *nbuf;
	struct custom_class_data *data = dev->data;
	struct usbd_class_data *const c_data = data->class_node;

	nbuf = usbd_ep_buf_alloc(c_data, data->desc->ep1in.bEndpointAddress, len);
	if (unlikely(!nbuf)) {
		LOG_ERR("%s, Couldnot alloc epbuf!", __func__);
		return -ENOMEM;
	}
	net_buf_add_mem(nbuf, buf, len);
	if (len && len % FS_MPS == 0)
    	udc_ep_buf_set_zlp(nbuf);

    k_mutex_lock(&data->lock, K_FOREVER);
	ret = usbd_ep_enqueue(c_data, nbuf);
    k_mutex_unlock(&data->lock);
	if (ret) {
		LOG_ERR("Couldnot enqueue tx");
		usbd_ep_buf_free(usbd_class_get_ctx(c_data), nbuf);
	}
	return ret;
}

static void custom_class_set_rx(const struct device *dev, usb_rxcb_t cb)
{
	struct custom_class_data *data = dev->data;
	k_mutex_lock(&data->lock, K_FOREVER);
	data->rx_callback = cb;
	k_mutex_unlock(&data->lock);
}

static const struct custom_class_api my_api = {
	.send = custom_class_do_send,
	.set_rx_callback = custom_class_set_rx
};

static int custom_class_start_receiving(struct usbd_class_data *const c_data)
{
	int ret;
	struct net_buf *buf;
	struct custom_class_data *data = usbd_class_get_private(c_data);
	struct custom_class_desc *desc = data->desc;

	buf = usbd_ep_buf_alloc(c_data, desc->ep1out.bEndpointAddress, FS_MPS);
	if (unlikely(!buf)) {
		LOG_ERR("%s, Couldnot alloc epbuf!", __func__);
		return -ENOMEM;
	}
	ret = usbd_ep_enqueue(c_data, buf);
	if (unlikely(ret)) {
		LOG_ERR("Couldnot enqueue rx");
		usbd_ep_buf_free(usbd_class_get_ctx(c_data), buf);
	}
	return ret;
}

static int custom_class_request(struct usbd_class_data *const c_data, struct net_buf *buf, int err)
{
	struct udc_buf_info *binfo = net_buf_user_data(buf);
	struct custom_class_data *data = usbd_class_get_private(c_data);
	struct custom_class_desc *desc = data->desc;
	const uint8_t ep = binfo->ep;
	usb_rxcb_t cb;

	if (err) {
		LOG_ERR("Transfer ep 0x%02x, failed", ep);
		usbd_ep_buf_free(usbd_class_get_ctx(c_data), buf);
		if (ep == desc->ep1out.bEndpointAddress)
			custom_class_start_receiving(c_data);
		return err;
	}

	if (ep == desc->ep1out.bEndpointAddress) {
		custom_class_start_receiving(c_data);
		k_mutex_lock(&data->lock, K_FOREVER);
		cb = data->rx_callback;
		k_mutex_unlock(&data->lock);
		if (cb)
			cb(buf->data, MIN(buf->len, FS_MPS));
	}

	return usbd_ep_buf_free(usbd_class_get_ctx(c_data), buf);
}

static int custom_class_init(struct usbd_class_data *const c_data)
{
	ARG_UNUSED(c_data);
	return 0;
}

static void *custom_class_get_desc(struct usbd_class_data *const c_data, const enum usbd_speed speed)
{
	struct custom_class_data *data = usbd_class_get_private(c_data);
	if (speed != USBD_SPEED_FS)
		return NULL;
	/* Only FS */
	return data->fs_desc;
}

static void custom_class_enable(struct usbd_class_data *const c_data)
{
	custom_class_start_receiving(c_data);
}

static void custom_class_disable(struct usbd_class_data *const c_data)
{
	ARG_UNUSED(c_data);
}

static const struct usbd_class_api custom_class_class_api = {
	.request = custom_class_request,
	.init = custom_class_init,
	.get_desc = custom_class_get_desc,
	.enable = custom_class_enable,
	.disable = custom_class_disable,
};

static struct custom_class_desc custom_class_descriptor = {
	.intf = {
		.bLength = sizeof(struct usb_if_descriptor),
		.bDescriptorType = USB_DESC_INTERFACE,
		.bInterfaceNumber = 0,
		.bAlternateSetting = 0,
		.bNumEndpoints = NUM_ENDPOINTS,
		.bInterfaceClass = USB_BCC_VENDOR,
		.bInterfaceSubClass = USB_BCC_VENDOR,
		.bInterfaceProtocol = USB_BCC_VENDOR,
		.iInterface = 0,
	},
	.ep1out = {
		.bLength = sizeof(struct usb_ep_descriptor),
		.bDescriptorType = USB_DESC_ENDPOINT,
		.bEndpointAddress = USB_EP_DIR_OUT | 1,
		.bmAttributes = USB_EP_TYPE_BULK,
		.wMaxPacketSize = sys_cpu_to_le16(FS_MPS),
		.bInterval = 0,
	},
	.ep1in = {
		.bLength = sizeof(struct usb_ep_descriptor),
		.bDescriptorType = USB_DESC_ENDPOINT,
		.bEndpointAddress = USB_EP_DIR_IN | 1,
		.bmAttributes = USB_EP_TYPE_BULK,
		.wMaxPacketSize = sys_cpu_to_le16(FS_MPS),
		.bInterval = 0,
	},
	.nil = {
		.bLength = 0,
		.bDescriptorType = 0,
	},
};

static const struct usb_desc_header *custom_class_fs_desc[] = {
	(struct usb_desc_header *)&custom_class_descriptor.intf,
	(struct usb_desc_header *)&custom_class_descriptor.ep1out,
	(struct usb_desc_header *)&custom_class_descriptor.ep1in,
	(struct usb_desc_header *)&custom_class_descriptor.nil,
};

static struct custom_class_data custom_class_data_inst = {
	.desc = &custom_class_descriptor,
	.fs_desc = custom_class_fs_desc,
	.rx_callback = NULL,
};

USBD_DEFINE_CLASS(custom_class_0, &custom_class_class_api, &custom_class_data_inst, NULL);

static int custom_class_abstract_init(const struct device *dev)
{
	struct custom_class_data *data = dev->data;
	data->class_node = &custom_class_0;
	return k_mutex_init(&data->lock);
}

DEVICE_DEFINE(custom_class_abstract, "custom_class", custom_class_abstract_init, NULL, &custom_class_data_inst, NULL, \
	POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &my_api);
