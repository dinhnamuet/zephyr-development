#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "wdt.h"

LOG_MODULE_REGISTER(DinhNamUET);

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
