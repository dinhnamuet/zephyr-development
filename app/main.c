#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/logging/log.h>

#include "wdt.h"
#include "joystick_hal.h"

LOG_MODULE_REGISTER(DinhNamUET);

ZBUS_CHAN_DECLARE(joystick_chan);
static void listener_callback(const struct zbus_channel *chan)
{
    const struct joystick_data *data;
    data = zbus_chan_const_msg(chan);
    LOG_INF("Joystick (%d, %d)", data->x, data->y);
}
ZBUS_LISTENER_DEFINE(joystick_listener, listener_callback);
ZBUS_CHAN_ADD_OBS(joystick_chan, joystick_listener, 3);

int main(void)
{
    watchdog_daemon_start(CONFIG_IWDG_STM32_INITIAL_TIMEOUT - 1000);
    LOG_INF("Enter %s", __func__);

    while (true) {
        LOG_INF("Hello, World!");
        k_msleep(5000);
    }
    return 0;
}
