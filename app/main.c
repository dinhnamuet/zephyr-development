#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/logging/log.h>

#include "wdt.h"
#include "tsensor.h"
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

static const struct device *const flash = DEVICE_DT_GET(DT_NODELABEL(spi_flash));

int main(void)
{
    uint64_t flash_size;
    uint8_t flash_data[3];
    const struct flash_parameters *param;

    watchdog_init(30000);
    watchdog_daemon_start(29000);
    tsensor_polling_start(20000);

    if (!device_is_ready(flash)) {
        LOG_ERR("Flash %s not found!", flash->name);
        return -ENODEV;
    }

    if (!flash_get_size(flash, &flash_size)) {
        LOG_INF("%s, size: %lld", flash->name, flash_size);
    }
    param = flash_get_parameters(flash);
    if (param) {
        LOG_INF("%s, wr_blk_size: %d", flash->name, param->write_block_size);
        LOG_INF("%s, erase_value: %d", flash->name, param->erase_value);
    }
    if (!flash_read(flash, 0, flash_data, 3)) {
        LOG_INF("%s, first 3 bytes: 0x%x%x%x", flash->name, flash_data[0], flash_data[1], flash_data[2]);
    }

    while (true) {
        k_msleep(5000);
    }
    return 0;
}
