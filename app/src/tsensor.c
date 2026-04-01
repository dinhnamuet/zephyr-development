#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/devicetree.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/logging/log.h>

#include "tsensor.h"

LOG_MODULE_REGISTER(temp_sensor);

struct sensor_priv {
    k_timeout_t polling_interval_ms;
    struct k_work_delayable work;
};

static bool temp_verify(const void *msg, size_t size)
{
    const struct sensor_data *sdata = msg;
    return (sdata->temperature > 0);
}

ZBUS_CHAN_DEFINE(temp_sensor_chan, struct sensor_data, \
    temp_verify, NULL, ZBUS_OBSERVERS_EMPTY, \
    ZBUS_MSG_INIT(.time_stamp_seconds = 0, .temperature = 0.0f) \
);

static const struct device *const sensor = DEVICE_DT_GET(DT_NODELABEL(die_temp));

static double cpu_get_temperature()
{
    struct sensor_value temp;

    if (!device_is_ready(sensor)) {
        return 0.0f;
    }
    if (sensor_sample_fetch(sensor)) {
        return 0.0f;
    }
    if (sensor_channel_get(sensor, SENSOR_CHAN_DIE_TEMP, &temp)) {
        return 0.0f;
    }
    
    return sensor_value_to_double(&temp);
}

static void workqueue_handler(struct k_work *work)
{
    struct sensor_data data;
    struct k_work_delayable *wdl = k_work_delayable_from_work(work);
    struct sensor_priv *priv = CONTAINER_OF(wdl, struct sensor_priv, work);

    data.temperature = cpu_get_temperature();
    data.time_stamp_seconds = k_uptime_seconds();
    zbus_chan_pub(&temp_sensor_chan, &data, K_FOREVER);
    k_work_reschedule(&priv->work, priv->polling_interval_ms);
}

int tsensor_polling_start(const uint32_t polling_interval_ms)
{
    struct sensor_priv *priv;
    if (!device_is_ready(sensor)) {
        LOG_ERR("Sensor not found");
        return -ENODEV;
    }
    priv = k_malloc(sizeof(*priv));
    if (!priv) {
        LOG_ERR("Out of heap space");
        return -ENOMEM;
    }
    priv->polling_interval_ms = K_MSEC(polling_interval_ms);
    k_work_init_delayable(&priv->work, workqueue_handler);
    k_work_schedule(&priv->work, priv->polling_interval_ms);
    return 0;
}
