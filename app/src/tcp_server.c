#include <zephyr/kernel.h>
#include <zephyr/sys/fdtable.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_compat.h>
#include <zephyr/logging/log.h>

#include "tcp_server.h"

LOG_MODULE_REGISTER(tcp_server);

#define MTU_SIZE           255
#define POLL_TIMEOUT_MS    10
#define MAX_CLIENTS        CONFIG_ZVFS_POLL_MAX

K_THREAD_STACK_DEFINE(tcp_server_stack, 4096);
K_THREAD_STACK_DEFINE(tcp_proc_stack, 4096);

struct client_slot {
    int fd;
    bool active;
};

struct {
    int server_fd;
    atomic_t num_active;
    struct client_slot slots[MAX_CLIENTS];
    struct k_thread conn_thread;
    struct k_thread proc_thread;
    struct k_mutex lock;
} server;

static void remove_client(int client_fd)
{
    k_mutex_lock(&server.lock, K_FOREVER);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (server.slots[i].fd == client_fd) {
            server.slots[i].active = false;
            atomic_sub(&server.num_active, 1);
            zsock_close(client_fd);
            break;
        }
    }
    k_mutex_unlock(&server.lock);
}

static void add_client(int client_fd)
{
    k_mutex_lock(&server.lock, K_FOREVER);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!server.slots[i].active) {
            server.slots[i].fd = client_fd;
            server.slots[i].active = true;
            atomic_add(&server.num_active, 1);
            k_mutex_unlock(&server.lock);
            return;
        }
    }
    k_mutex_unlock(&server.lock);
    zsock_close(client_fd);
    LOG_WRN("Add failed, remove client %d", client_fd);
}

static void tcp_server_handler(void *p1, void *, void *)
{
    int res, num_active;
    tcp_rx_t cb = p1;
    uint8_t buf[MTU_SIZE];
    struct zsock_pollfd pollfd[MAX_CLIENTS];
    struct zsock_pollfd *pollfd_ptr;

    while (true) {
        if (!atomic_get(&server.num_active)) {
            k_msleep(POLL_TIMEOUT_MS);
            continue;
        }
        k_mutex_lock(&server.lock, K_FOREVER);
        pollfd_ptr = pollfd;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (server.slots[i].active) {
                pollfd_ptr->fd = server.slots[i].fd;
                pollfd_ptr->events = ZSOCK_POLLIN;
                pollfd_ptr++->revents = 0;
            }
        }
        num_active = atomic_get(&server.num_active);
        k_mutex_unlock(&server.lock);
        res = zsock_poll(pollfd, num_active, POLL_TIMEOUT_MS);
        if (res <= 0) {
            // LOG_WRN_RATELIMIT("Poll %s", (res < 0) ? "error" : "timeout");
        } else {
            for (int i = 0; i < num_active; i++) {
                if ((pollfd[i].revents & ZSOCK_POLLHUP) == ZSOCK_POLLHUP) {
                    remove_client(pollfd[i].fd);
                } else if ((pollfd[i].revents & ZSOCK_POLLIN) == ZSOCK_POLLIN) {
                    res = zsock_recv(pollfd[i].fd, buf, MTU_SIZE, ZSOCK_MSG_DONTWAIT);
                    if (res > 0 && cb) {
                        cb(pollfd[i].fd, buf, res);
                    } else if (res == 0) {
                        remove_client(pollfd[i].fd);
                    }
                }
            }
        }
    }
}

static void tcp_server_conn_handler(void *, void *, void *)
{
    int client_fd;
    char buf[NET_IPV4_ADDR_LEN];
    struct net_sockaddr_in client_address;
    socklen_t client_addr_len = sizeof(client_address);

    while (true) {
        client_fd = zsock_accept(server.server_fd, (struct net_sockaddr *)&client_address, &client_addr_len);
        if (client_fd < 0) {
            LOG_WRN("Accept error, ignore client");
            continue;
        }
        add_client(client_fd);
        net_addr_ntop(AF_INET, &client_address.sin_addr, buf, sizeof(buf));
        LOG_INF("Client %s connected, fd = %d", buf, client_fd);
    }
}

int tcp_server_init(uint16_t port)
{
    struct net_sockaddr_in server_addr;
    server.server_fd = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server.server_fd < 0) {
        LOG_ERR("Socket error");
        return server.server_fd;
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server.num_active = ATOMIC_INIT(0);
    k_mutex_init(&server.lock);
    return zsock_bind(server.server_fd, (struct net_sockaddr *)&server_addr, sizeof(server_addr));
}

int tcp_server_start(tcp_rx_t cb)
{
    int ret;
    ret = zsock_listen(server.server_fd, MAX_CLIENTS);
    if (ret) {
        LOG_ERR("Listen error");
        return ret;
    }
    k_thread_create(&server.conn_thread, tcp_server_stack, K_THREAD_STACK_SIZEOF(tcp_server_stack),
                    tcp_server_conn_handler, NULL, NULL, NULL, 5, 0, K_NO_WAIT);
    k_thread_create(&server.proc_thread, tcp_proc_stack, K_THREAD_STACK_SIZEOF(tcp_proc_stack),
                    tcp_server_handler, (void *)cb, NULL, NULL, 5, 0, K_NO_WAIT);
    return 0;
}

ssize_t tcp_server_send(int fd, const uint8_t *buf, size_t len)
{
    return zsock_send(fd, buf, len, ZSOCK_MSG_DONTWAIT);
}
