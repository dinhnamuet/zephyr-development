#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/dhcpv4.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/http/server.h>
#include <zephyr/mgmt/updatehub.h>
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

LOG_MODULE_REGISTER(DinhNamUET);

#define TCP_PORT       2026
#define CONN_TIMEOUT   5000
#define CONN_NR_TRIES  5

K_SEM_DEFINE(wifi_conn, 0, 1);
K_SEM_DEFINE(net_ready, 0, 1);
static char ipv4[NET_IPV4_ADDR_LEN];
static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback mgmt_cb;

static void wifi_callback(struct net_mgmt_event_callback *cb, uint64_t evt, struct net_if *intf)
{
    switch (evt) {
    case NET_EVENT_WIFI_CONNECT_RESULT:
        k_sem_give(&wifi_conn);
        break;

    case NET_EVENT_WIFI_DISCONNECT_RESULT:
        LOG_WRN("%s disconnected", net_if_get_device(intf)->name);
        break;
    
    default:
        break;
    }
}

static void net_evt_callback(struct net_mgmt_event_callback *cb, uint64_t evt, struct net_if *intf)
{
    switch (evt) {
    case NET_EVENT_L4_CONNECTED:
        struct net_in_addr *addr = net_if_ipv4_get_global_addr(intf, NET_ADDR_PREFERRED);
        if (addr) {
            net_addr_ntop(AF_INET, addr, ipv4, sizeof(ipv4));
            LOG_INF("IPv4: %s", ipv4);
        }
        k_sem_give(&net_ready);
        break;

    default:
        break;
    }
}

static void tcp_received(int fd, const uint8_t *buf, size_t size)
{
    dfu_handle(fd, buf[0], &buf[1], size - 1);
}

int main(void)
{
    uint8_t tried;
    static uint8_t ssid[20];
    static uint8_t psk[20];
    uint8_t ssid_length, psk_length;
    struct net_if *intf;
    struct wifi_connect_req_params parms;

    boot_write_img_confirmed();
    watchdog_init(30000);
    watchdog_daemon_start(29000);

    storage_init();
    ssid_length = storage_read(WIFI_SSID_ID, NULL, 0);
    psk_length = storage_read(WIFI_PSK_ID, NULL, 0);
    storage_read(WIFI_SSID_ID, ssid, ssid_length);
    storage_read(WIFI_PSK_ID, psk, psk_length);

    memset(&parms, 0, sizeof(parms));
    parms.ssid = ssid;
    parms.ssid_length = ssid_length;
    parms.psk = psk;
    parms.psk_length = psk_length;
    parms.band = WIFI_FREQ_BAND_2_4_GHZ;
    parms.channel = WIFI_CHANNEL_ANY;
    parms.security = WIFI_SECURITY_TYPE_PSK;
    parms.mfp = WIFI_MFP_DISABLE;
    parms.timeout = SYS_FOREVER_MS;
    intf = net_if_get_wifi_sta();
    if (!intf) {
        LOG_ERR("Wifi interface not found");
        return -EFAULT;
    }
    net_mgmt_init_event_callback(&wifi_cb, wifi_callback, NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT);
    net_mgmt_add_event_callback(&wifi_cb);
    net_mgmt_init_event_callback(&mgmt_cb, net_evt_callback, NET_EVENT_L4_CONNECTED);
    net_mgmt_add_event_callback(&mgmt_cb);

    for (tried = 0; tried < CONN_NR_TRIES; tried++) {
        LOG_INF("%s connecting to %s (%d/%d)...!", net_if_get_device(intf)->name, parms.ssid, tried + 1, CONN_NR_TRIES);
		net_mgmt(NET_REQUEST_WIFI_CONNECT, intf, &parms, sizeof(parms));

        if (!k_sem_take(&wifi_conn, K_MSEC(CONN_TIMEOUT))) {
            break;
        }
		LOG_INF("Connect request failed. Waiting %s be up!", net_if_get_device(intf)->name);
		k_msleep(500);
	}
    if (tried >= CONN_NR_TRIES) {
        LOG_ERR("Failed");
        return -EFAULT;
    }
    net_dhcpv4_start(intf);
    LOG_INF("Connected, waiting network to ready");
    k_sem_take(&net_ready, K_FOREVER);
    updatehub_autohandler();

    if (tcp_server_init(TCP_PORT)) {
        LOG_ERR("TCP Init failed");
        return -EFAULT;
    }
    if (tcp_server_start(tcp_received)) {
        LOG_ERR("TCP Server start failed");
        return -EFAULT;
    }
    LOG_INF("TCP Server Listening at port %d", TCP_PORT);
    if (http_server_start() < 0) {
        LOG_ERR("HTTP Server start failed");
        return -EFAULT;
    }
    LOG_INF("HTTP server started on port 80");
    LOG_INF("Open browser: http://%s/", ipv4);

    while (true) {
        k_sleep(K_SECONDS(10));
    }
    return 0;
}
