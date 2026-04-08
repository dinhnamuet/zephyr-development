#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/dhcpv4.h>
#include <zephyr/net/net_ip.h>
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
#define WIFI_MASK      (NET_EVENT_WIFI_CONNECT_RESULT | \
                        NET_EVENT_WIFI_DISCONNECT_RESULT)

static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback ipv4_cb;
K_SEM_DEFINE(wifi_conn, 0, 1);

static void wifi_callback(struct net_mgmt_event_callback *cb, uint64_t evt, struct net_if *intf)
{
    if (evt == NET_EVENT_WIFI_CONNECT_RESULT) {
        net_dhcpv4_start(intf);
    } else if (evt == NET_EVENT_WIFI_DISCONNECT_RESULT) {
        LOG_INF("Disconnected");
    } else if (evt == NET_EVENT_IPV4_ADDR_ADD) {
        struct net_in_addr *addr = net_if_ipv4_get_global_addr(intf, NET_ADDR_PREFERRED);
        if (addr) {
            char buf[NET_IPV4_ADDR_LEN];
            net_addr_ntop(AF_INET, addr, buf, sizeof(buf));
            printk("IPv4: %s\n", buf);
        }
        k_sem_give(&wifi_conn);
    }
}

static void tcp_received(int fd, const uint8_t *buf, size_t size)
{
    LOG_INF("fd %d: %s", fd,  (char *)buf);
    tcp_server_send(fd, buf, size);
}

int main(void)
{
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
    net_mgmt_init_event_callback(&wifi_cb, wifi_callback, WIFI_MASK);
    net_mgmt_add_event_callback(&wifi_cb);
    net_mgmt_init_event_callback(&ipv4_cb, wifi_callback, NET_EVENT_IPV4_ADDR_ADD);
    net_mgmt_add_event_callback(&ipv4_cb);

    int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, intf, &parms, sizeof(parms));
    if (ret) {
        LOG_ERR("Wifi connection failed: %d", ret);
        return -EFAULT;
    }
    LOG_INF("%s connecting to %s ...!", net_if_get_device(intf)->name, parms.ssid);
    k_sem_take(&wifi_conn, K_FOREVER);
    LOG_INF("Connected");

    if (tcp_server_init(TCP_PORT)) {
        LOG_ERR("TCP Init failed");
        return -EFAULT;
    }
    if (tcp_server_start(tcp_received)) {
        LOG_ERR("TCP Server start failed");
        return -EFAULT;
    }
    LOG_INF("TCP Server Listening at port %d", TCP_PORT);

    while (true) {
        k_sleep(K_SECONDS(10));
    }
    return 0;
}
