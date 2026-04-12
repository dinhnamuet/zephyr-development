#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/net/http/server.h>
#include <zephyr/logging/log.h>

#include "wdt.h"
#include "usb.h"
#include "dfu.h"
#include "counter.h"
#include "tsensor.h"
#include "storage.h"
#include "serial.h"
#include "joystick_hal.h"
#include "tcp_server.h"
#include "updatehub_ota.h"
#include "wifi_impl.h"

LOG_MODULE_REGISTER(DinhNamUET);

static void tcp_received(int client_fd, const uint8_t *buf, size_t size)
{
    dfu_handle(client_fd, buf[0], &buf[1], size - 1);
}

int main(void)
{
    boot_write_img_confirmed();
    watchdog_init(30000);
    watchdog_daemon_start(29000);
    wifi_init();
    updatehub_init();

    wifi_connect();
    LOG_INF("IPv4: %s", wifi_get_ipv4());
    updatehub_start();
    tcp_server_init(2026);
    tcp_server_start(tcp_received);

    while (true) {
        k_sleep(K_SECONDS(10));
    }
    return 0;
}
