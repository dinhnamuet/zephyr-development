#define DT_DRV_COMPAT nam_joystick

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>
#include <zephyr/input/input.h>

LOG_MODULE_REGISTER(joystick_driver, CONFIG_INPUT_LOG_LEVEL);

struct joystick_config {
    const struct adc_dt_spec adcx;
    const struct adc_dt_spec adcy;
    const uint32_t polling_interval_ms;
};

struct joystick_data {
	uint16_t             raw_x;
	uint16_t             raw_y;
	struct adc_sequence  seq_x;
	struct adc_sequence  seq_y;
	const struct device *dev;
	struct k_work_delayable work;
};

static void joystick_poll(struct k_work *w)
{
	int ret;
	struct k_work_delayable *dwork = k_work_delayable_from_work(w);
	struct joystick_data *data = CONTAINER_OF(dwork, struct joystick_data, work);
	const struct joystick_config *cfg = data->dev->config;

	ret = adc_read_dt(&cfg->adcx, &data->seq_x);
	if (ret < 0) {
		LOG_ERR("ADC X read failed: %d", ret);
		goto reschedule;
	}

	ret = adc_read_dt(&cfg->adcy, &data->seq_y);
	if (ret < 0) {
		LOG_ERR("ADC Y read failed: %d", ret);
		goto reschedule;
	}

	input_report_abs(data->dev, INPUT_ABS_X, data->raw_x, false, K_FOREVER);
	input_report_abs(data->dev, INPUT_ABS_Y, data->raw_y, true,  K_FOREVER);

reschedule:
	k_work_reschedule(&data->work, K_MSEC(cfg->polling_interval_ms));
}

static int joystick_init(const struct device *dev)
{
    int ret;
    const struct joystick_config *cfg = dev->config;
    struct joystick_data *data = dev->data;
    data->dev = dev;

	if (!adc_is_ready_dt(&cfg->adcx)) {
		LOG_ERR("Axis X device (%s) not ready", cfg->adcx.dev->name);
		return -ENODEV;
	}
	if (!adc_is_ready_dt(&cfg->adcy)) {
		LOG_ERR("Axis Y device (%s) not ready", cfg->adcy.dev->name);
		return -ENODEV;
	}

    ret = adc_channel_setup_dt(&cfg->adcx);
    if (ret) {
        LOG_ERR("Couldnot setup adc for X Axis");
        return ret;
    }
    ret = adc_channel_setup_dt(&cfg->adcy);
    if (ret) {
        LOG_ERR("Couldnot setup adc for Y Axis");
        return ret;
    }

    ret = adc_sequence_init_dt(&cfg->adcx, &data->seq_x);
	if (ret < 0) {
		LOG_ERR("Failed to init ADC X sequence: %d", ret);
		return ret;
	}
	data->seq_x.buffer      = &data->raw_x;
	data->seq_x.buffer_size = sizeof(data->raw_x);

	ret = adc_sequence_init_dt(&cfg->adcy, &data->seq_y);
	if (ret < 0) {
		LOG_ERR("Failed to init ADC Y sequence: %d", ret);
		return ret;
	}
	data->seq_y.buffer      = &data->raw_y;
	data->seq_y.buffer_size = sizeof(data->raw_y);

    LOG_INF("ADC X: dev=%s ch=%d vref=%dmV res=%d gain=%d ref=%d acq=%d",
		cfg->adcx.dev->name,
		cfg->adcx.channel_id,
		cfg->adcx.vref_mv,
		cfg->adcx.resolution,
		cfg->adcx.channel_cfg.gain,
		cfg->adcx.channel_cfg.reference,
		cfg->adcx.channel_cfg.acquisition_time);

	LOG_INF("ADC Y: dev=%s ch=%d vref=%dmV res=%d gain=%d ref=%d acq=%d",
		cfg->adcy.dev->name,
		cfg->adcy.channel_id,
		cfg->adcy.vref_mv,
		cfg->adcy.resolution,
		cfg->adcy.channel_cfg.gain,
		cfg->adcy.channel_cfg.reference,
		cfg->adcy.channel_cfg.acquisition_time);

    LOG_INF("Polling interval: %dms", cfg->polling_interval_ms);

    k_work_init_delayable(&data->work, joystick_poll);
    k_work_schedule(&data->work, K_MSEC(cfg->polling_interval_ms));

    return 0;
}

#define JOYSTICK_INST(n)                                                                                        \
    static struct joystick_data joystick_data_##n;                                                              \
                                                                                                                \
    static const struct joystick_config joystick_config_##n = {                                                 \
        .adcx = ADC_DT_SPEC_INST_GET_BY_NAME(n, adcx),                                                          \
        .adcy = ADC_DT_SPEC_INST_GET_BY_NAME(n, adcy),                                                          \
        .polling_interval_ms = DT_INST_PROP(n, polling_interval_ms),                                            \
    };                                                                                                          \
                                                                                                                \
    DEVICE_DT_INST_DEFINE(n, joystick_init, NULL, &joystick_data_##n,                                           \
    &joystick_config_##n, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,                                      \
    NULL);

DT_INST_FOREACH_STATUS_OKAY(JOYSTICK_INST);
