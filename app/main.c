#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/logging/log.h>

#include "wdt.h"
#include "usb.h"
#include "dfu.h"
#include "tsensor.h"
#include "storage.h"
#include "serial.h"
#include "joystick_hal.h"

LOG_MODULE_REGISTER(DinhNamUET);

ZBUS_CHAN_DECLARE(joystick_chan);
ZBUS_CHAN_DECLARE(temp_sensor_chan);

static void listener_callback(const struct zbus_channel *chan)
{
    if (chan == &joystick_chan) {
        const struct joystick_data *data;
        data = zbus_chan_const_msg(chan);
        LOG_INF("Joystick (%d, %d)", data->x, data->y);
    } else if (chan == &temp_sensor_chan) {
        const struct sensor_data *data;
        data = zbus_chan_const_msg(chan);
        LOG_INF("L: %.2f°C, at %ds", data->temperature, data->time_stamp_seconds);
    }
}
ZBUS_LISTENER_DEFINE(joystick_listener, listener_callback);
ZBUS_LISTENER_DEFINE(tsensor_listener, listener_callback);
ZBUS_CHAN_ADD_OBS(joystick_chan, joystick_listener, 3);
ZBUS_CHAN_ADD_OBS(temp_sensor_chan, tsensor_listener, 3);

static void usb_received(const uint8_t *data, size_t size)
{
    dfu_handle(data[0], &data[1], size - 1);
}

int main(void)
{
    struct mcuboot_img_header header;

    boot_write_img_confirmed();
    watchdog_init(30000);
    watchdog_daemon_start(29000);
    tsensor_polling_start(20000);
    usb_app_init(usb_received);

    if (boot_read_bank_header(boot_fetch_active_slot(), &header, sizeof(struct mcuboot_img_header))) {
        LOG_ERR("ERROR");
        return -EFAULT;
    }
    struct mcuboot_img_sem_ver *semver = &header.h.v1.sem_ver;
    LOG_INF("IVersion %d.%d.%d-%d", semver->major, semver->minor, semver->revision, semver->build_num);
    
    return 0;
}
