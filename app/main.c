#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/logging/log.h>

#include "wdt.h"
#include "usb.h"
#include "dfu.h"
#include "counter.h"
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

static int counter_cb(uint8_t chan_id, uint32_t us)
{
    static unsigned char count;
    LOG_INF("%d, %dms", chan_id, us/1000);
    if (count++ > 10)
        ct_stop();
    return ALARM_REPEAT;
}

int main(void)
{
    boot_write_img_confirmed();
    watchdog_init(30000);
    watchdog_daemon_start(29000);
    tsensor_polling_start(20000);
    ct_init();
    ct_set_alarm(0, 5000000, counter_cb);
    ct_set_alarm(1, 2000000, counter_cb);
    ct_set_alarm(2, 3000000, counter_cb);
    ct_set_alarm(3, 4000000, counter_cb);
    ct_start();

    return 0;
}
