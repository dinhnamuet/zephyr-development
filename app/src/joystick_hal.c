#include <zephyr/kernel.h>
#include <zephyr/input/input.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/logging/log.h>

#include "joystick_hal.h"

LOG_MODULE_REGISTER(joystick_hal);

static struct joystick_data data;
static const struct device *const joystick = DEVICE_DT_GET(DT_NODELABEL(my_joystick));

ZBUS_CHAN_DEFINE(joystick_chan, struct joystick_data, NULL,
    NULL, ZBUS_OBSERVERS_EMPTY, ZBUS_MSG_INIT(.x = 0, .y = 0)
);

static void joystick_callback(struct input_event *evt, void *userdata)
{
    struct zbus_channel *channel = userdata;
    if (evt->code == INPUT_ABS_X) {
        data.x = evt->value;
    } else if (evt->code == INPUT_ABS_Y) {
        data.y = evt->value;
    }
    if (evt->sync) {
        if (zbus_chan_pub(channel, &data, K_NO_WAIT)) {
            LOG_WRN("Publish data failed, drop message");
        }
    }
}
INPUT_CALLBACK_DEFINE(joystick, joystick_callback, (void *)&joystick_chan);
