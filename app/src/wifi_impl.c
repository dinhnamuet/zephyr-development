#include <zephyr/kernel.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/dhcpv4.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(wifi_impl);

#define CONN_TIMEOUT          5000
#define CONN_NR_TRIES         5

K_SEM_DEFINE(wifi_conn, 0, 1);
K_SEM_DEFINE(net_ready, 0, 1);
K_SEM_DEFINE(wifi_conf, 0, 1);

static char ipv4[NET_IPV4_ADDR_LEN];
static char saved_ssid[WIFI_SSID_MAX_LEN];
static char saved_psk[WIFI_PSK_MAX_LEN];
static struct net_if *intf;
static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback mgmt_cb;
static struct wifi_connect_req_params wifi_parms;

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
        }
        k_sem_give(&net_ready);
        break;

    default:
        break;
    }
}

static int wifi_settings_set(const char *name, size_t len, settings_read_cb rcb, void *args)
{
    const char *next;

    if (settings_name_steq(name, "ssid", &next) && !next) {
        rcb(args, &saved_ssid, sizeof(saved_ssid));
        return 0;
    }
    if (settings_name_steq(name, "psk", &next) && !next) {
        rcb(args, &saved_psk, sizeof(saved_psk));
        return 0;
    }
    return -ENOENT;
}

static int wifi_settings_commit(void)
{
    k_sem_give(&wifi_conf);
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(wifi, "wifi", NULL, wifi_settings_set, wifi_settings_commit, NULL);

static int wifi_fetch(void)
{
    settings_subsys_init();
    settings_load_subtree("wifi");
    return k_sem_take(&wifi_conf, K_MSEC(1000));
}

int wifi_init(void)
{
    intf = net_if_get_wifi_sta();
    if (!intf) {
        LOG_ERR("Wifi interface not found");
        return -ENODEV;
    }
    if (wifi_fetch()) {
        LOG_ERR("wifi default not found");
        return -EINVAL;
    }

    wifi_parms.ssid = saved_ssid;
    wifi_parms.ssid_length = strlen(saved_ssid);
    wifi_parms.psk = saved_psk;
    wifi_parms.psk_length = strlen(saved_psk);
    wifi_parms.band = WIFI_FREQ_BAND_2_4_GHZ;
    wifi_parms.channel = WIFI_CHANNEL_ANY;
    wifi_parms.security = WIFI_SECURITY_TYPE_PSK;
    wifi_parms.mfp = WIFI_MFP_DISABLE;
    wifi_parms.timeout = SYS_FOREVER_MS;

    net_mgmt_init_event_callback(&wifi_cb, wifi_callback, NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT);
    net_mgmt_add_event_callback(&wifi_cb);
    net_mgmt_init_event_callback(&mgmt_cb, net_evt_callback, NET_EVENT_L4_CONNECTED);
    net_mgmt_add_event_callback(&mgmt_cb);

    return 0;
}

static int wifi_connect_default(void)
{
    int tried;
    for (tried = 0; tried < CONN_NR_TRIES; tried++) {
        LOG_INF("%s connecting to %s (%d/%d)...!", net_if_get_device(intf)->name, wifi_parms.ssid, tried + 1, CONN_NR_TRIES);
		net_mgmt(NET_REQUEST_WIFI_CONNECT, intf, &wifi_parms, sizeof(wifi_parms));

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
    return 0;
}

int wifi_connect(const char *ssid, const char *password)
{
    if (ssid && password) {
        wifi_parms.ssid = ssid;
        wifi_parms.ssid_length = strlen(ssid);
        wifi_parms.psk = password;
        wifi_parms.psk_length = strlen(password);
    }
    return wifi_connect_default();
}

int wifi_save_default(const char *ssid, const char *password)
{
    int ret;
    ret = settings_save_one("wifi/ssid", ssid, strlen(ssid));
    if (ret) {
        LOG_ERR("SSID save failed");
        return ret;
    }
    ret = settings_save_one("wifi/psk", password, strlen(password));
    if (ret) {
        LOG_ERR("PSK save failed");
    }
    return ret;
}

const char *wifi_get_ipv4(void)
{
    return ipv4;
}
