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

int watchdog_init(unsigned long timeoutms)
{
    int ret;
    struct wdt_timeout_cfg cfg;

    if (!device_is_ready(wdt)) {
        LOG_ERR("Wdt %s not found", wdt->name);
        return -ENODEV;
    }
    
    cfg.callback = NULL;
    cfg.flags = 0;
    cfg.window.max = timeoutms;
    ret = wdt_install_timeout(wdt, &cfg);
    
    return (ret < 0) ? ret : wdt_setup(wdt, WDT_OPT_PAUSE_HALTED_BY_DBG);
}

void watchdog_daemon_start(unsigned long duration_ms)
{
    static struct k_timer timer;
    k_timer_init(&timer, feeder_periodic, NULL);
	k_timer_start(&timer, K_MSEC(duration_ms), K_MSEC(duration_ms));
}
