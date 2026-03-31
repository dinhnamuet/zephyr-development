#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

#include "wdt.h"

LOG_MODULE_REGISTER(wdt);

static const struct device *const wdt = DEVICE_DT_GET(DT_NODELABEL(iwdg1));

static void feeder_periodic(struct k_timer *)
{
    if (device_is_ready(wdt)) {
        wdt_feed(wdt, 0);
    }
}

void watchdog_daemon_start(unsigned long duration_ms)
{
    static struct k_timer timer;
    k_timer_init(&timer, feeder_periodic, NULL);
	k_timer_start(&timer, K_MSEC(duration_ms), K_MSEC(duration_ms));
}
