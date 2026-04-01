#pragma once

#define FS_MPS        64U

typedef void (*usb_rxcb_t)(const uint8_t *buf, size_t size);

struct custom_class_api {
    int (*send)(const struct device *dev, const uint8_t *buf, size_t len);
    void (*set_rx_callback)(const struct device *dev, usb_rxcb_t cb);
};

static inline int custom_class_send(const struct device *dev, const uint8_t *buf, size_t len)
{
    const struct custom_class_api *api = dev->api;
    return api->send(dev, buf, len);
}

static inline void custom_class_set_rx_callback(const struct device *dev,
                                            usb_rxcb_t cb)
{
    const struct custom_class_api *api = dev->api;
    api->set_rx_callback(dev, cb);
}
