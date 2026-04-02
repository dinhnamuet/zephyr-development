#include <zephyr/kernel.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/logging/log.h>

#include "counter.h"

LOG_MODULE_REGISTER(counter);

static const struct device *const counter = DEVICE_DT_GET(DT_NODELABEL(test_counter));

struct counter_info {
    uint8_t num_channels;
    uint32_t max_top;
    uint32_t current_top;
    uint32_t freq;
    struct counter_alarm_cfg *cfg;
} ct;

static void counter_alarm_callback(const struct device *dev, uint8_t chan_id, uint32_t ticks,
					 void *user_data)
{
    ct_cb_t cb = user_data;
    if (cb) {
        if (cb(chan_id, counter_ticks_to_us(counter, ticks)) == ALARM_REPEAT) {
            counter_set_channel_alarm(counter, chan_id, &ct.cfg[chan_id]);
        }
    }
}

int ct_init(void)
{
    if (!device_is_ready(counter)) {
        LOG_ERR("Counter %s not found", counter->name);
        return -ENODEV;
    }
    ct.num_channels = counter_get_num_of_channels(counter);
    ct.max_top = counter_get_max_top_value(counter);
    ct.current_top = counter_get_top_value(counter);
    ct.freq = counter_get_frequency(counter);
    ct.cfg = k_calloc(ct.num_channels, sizeof(struct counter_alarm_cfg));
    if (!ct.cfg) {
        LOG_ERR("Out of heap");
        return -ENOMEM;
    }
    for (int i = 0; i < ct.num_channels; i++) {
        ct.cfg[i].callback = counter_alarm_callback;
    }
    LOG_INF("%s, %d channels", counter->name, ct.num_channels);
    LOG_INF("count from 0 to 0x%x, freq: %dHz", ct.current_top, ct.freq);
    return 0;
}

void ct_destroy(void)
{
    counter_stop(counter);
    k_free(ct.cfg);
}

int ct_start(void)
{
    return counter_start(counter);
}

int ct_stop(void)
{
    return counter_stop(counter);
}

int ct_rst(void)
{
    return counter_reset(counter);
}

int ct_set_val(uint32_t ticks)
{
    return (ticks > ct.current_top) ? -EINVAL : counter_set_value(counter, ticks);
}

int ct_get_val(uint32_t *ticks)
{
    return counter_get_value(counter, ticks);
}

int ct_set_alarm(uint8_t chan_id, uint32_t us, ct_cb_t cb)
{
    if (chan_id >= ct.num_channels) {
        LOG_ERR("Valid id: 0 - %d", ct.num_channels - 1);
        return -EINVAL;
    }
    ct.cfg[chan_id].user_data = (void *)cb;
    ct.cfg[chan_id].ticks = counter_us_to_ticks(counter, us);
    return counter_set_channel_alarm(counter, chan_id, &ct.cfg[chan_id]);
}

int ct_remove_alarm(uint8_t chan_id)
{
    if (chan_id >= ct.num_channels) {
        LOG_ERR("Valid id: 0 - %d", ct.num_channels - 1);
        return -EINVAL;
    }
    ct.cfg[chan_id].user_data = NULL;
    return counter_cancel_channel_alarm(counter, chan_id);
}
