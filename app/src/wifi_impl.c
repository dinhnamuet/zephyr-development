#include <zephyr/kernel.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/dhcpv4.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/settings/settings.h>
#include <zephyr/net/wifi_credentials.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(wifi_impl);

#define CONN_TIMEOUT          5000
#define CONN_NR_TRIES         5

K_SEM_DEFINE(wifi_conn, 0, 1);
K_SEM_DEFINE(net_ready, 0, 1);

static char ipv4[NET_IPV4_ADDR_LEN];
static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback mgmt_cb;
static bool wifi_connected;

static void wifi_callback(struct net_mgmt_event_callback *cb, uint64_t evt, struct net_if *intf)
{
    switch (evt) {
    case NET_EVENT_WIFI_CONNECT_RESULT:
        k_sem_give(&wifi_conn);
        wifi_connected = true;
        break;

    case NET_EVENT_WIFI_DISCONNECT_RESULT:
        LOG_WRN("%s disconnected", net_if_get_device(intf)->name);
        wifi_connected = false;
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

static int wifi_connect(struct net_if *intf, struct wifi_connect_req_params *wifi_parms)
{
    int tried;
    for (tried = 0; tried < CONN_NR_TRIES; tried++) {
        LOG_INF("%s connecting to %s (%d/%d)...!", net_if_get_device(intf)->name, wifi_parms->ssid, tried + 1, CONN_NR_TRIES);
		net_mgmt(NET_REQUEST_WIFI_CONNECT, intf, (void *)wifi_parms, sizeof(*wifi_parms));

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

static void wifi_cred_ssid_cb(void *args, const char *ssid, size_t ssid_len)
{
    int ret;
    uint32_t flags;
    uint8_t psk[WIFI_PSK_MAX_LEN];
    struct net_if *intf = args;
    struct wifi_connect_req_params wifi_parms;

    if (wifi_connected) {
        return;
    }
    memset(&wifi_parms, 0, sizeof(wifi_parms));
    wifi_parms.ssid = ssid;
    wifi_parms.ssid_length = ssid_len;
    wifi_parms.psk = psk;
    ret = wifi_credentials_get_by_ssid_personal(ssid, ssid_len, &wifi_parms.security,
                wifi_parms.bssid, sizeof(wifi_parms.bssid), psk, sizeof(psk),
                (size_t *)&wifi_parms.psk_length, &flags, &wifi_parms.channel,
                &wifi_parms.timeout);
                
    if (!ret) {
        wifi_connect(intf, &wifi_parms);
    }
}

int wifi_init(void)
{
    struct net_if *intf = net_if_get_wifi_sta();
    if (!intf) {
        LOG_ERR("Wifi interface not found");
        return -ENODEV;
    }
    if (wifi_credentials_is_empty()) {
        LOG_ERR("Wifi list is empty");
        return -EINVAL;
    }

    net_mgmt_init_event_callback(&wifi_cb, wifi_callback, (NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT));
    net_mgmt_add_event_callback(&wifi_cb);
    net_mgmt_init_event_callback(&mgmt_cb, net_evt_callback, NET_EVENT_L4_CONNECTED);
    net_mgmt_add_event_callback(&mgmt_cb);

    wifi_credentials_for_each_ssid(wifi_cred_ssid_cb, intf);

    return 0;
}

int wifi_save(const char *ssid, const char *password)
{
    return wifi_credentials_set_personal(ssid, strlen(ssid), WIFI_SECURITY_TYPE_PSK,
                NULL, 0, password, strlen(password), WIFI_CREDENTIALS_FLAG_2_4GHz,
                WIFI_CHANNEL_ANY, SYS_FOREVER_MS);
}

int wifi_del(const char *ssid)
{
    return wifi_credentials_delete_by_ssid(ssid, strlen(ssid));
}

const char *wifi_get_ipv4(void)
{
    return ipv4;
}
