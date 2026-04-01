#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/logging/log.h>

#include "wdt.h"
#include "tsensor.h"
#include "storage.h"
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
    int integer_var;
    unsigned int boot_count;
    char string_var[17] = { 0 };

    watchdog_init(30000);
    watchdog_daemon_start(29000);
    tsensor_polling_start(20000);

    if (storage_init()) {
        LOG_ERR("Storage init failure");
        return -EFAULT;
    }

    if (storage_read(STRING_ID, string_var, 16) < 0) {
        storage_write(STRING_ID, "hello world!!!!!", 16);
    } else {
        LOG_INF("%s", string_var);
    }

    if (storage_read(INTEGER_ID, &integer_var, sizeof(int)) < 0) {
        integer_var = 36;
        storage_write(INTEGER_ID, &integer_var, sizeof(int));
    } else {
        LOG_INF("%d", integer_var);
    }

    if (storage_read(BOOT_COUNT_ID, &boot_count, sizeof(unsigned int)) < 0) {
        boot_count = 1;
        storage_write(BOOT_COUNT_ID, &boot_count, sizeof(unsigned int));
    } else {
        LOG_INF("%d", boot_count++);
        storage_write(BOOT_COUNT_ID, &boot_count, sizeof(unsigned int));
    }
    
    return 0;
}
