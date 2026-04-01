#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
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

int main(void)
{
    watchdog_daemon_start(CONFIG_IWDG_STM32_INITIAL_TIMEOUT - 1000);
    tsensor_polling_start(2000);
    LOG_INF("Enter %s", __func__);

    while (true) {
        LOG_INF("Hello, World!");
        k_msleep(5000);
    }
    return 0;
}
