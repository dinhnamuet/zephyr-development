#include <zephyr/kernel.h>
#include <zephyr/net/http/service.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/logging/log.h>

#include "joystick_hal.h"

LOG_MODULE_REGISTER(http_server);

static const uint8_t index_html[] = {
    "<html><head><title>Đinh Hữu Nam</title>"
    "<meta charset=\"UTF-8\">"
    "<style>body{font-family:Arial;margin:40px;background:#f0f4f8;}"
    ".card{background:white;padding:20px;border-radius:8px;"
    "box-shadow:0 2px 8px rgba(0,0,0,.1);}</style>"
    "</head><body>"
    "<div class=\"card\"><h1>dinhnamuet</h1>"
    "<p>Status: <b>Online</b></p>"
    "<p><a href=\"/api/sensor\">JoyStick</a></p>"
    "</div></body></html>"
};

ZBUS_CHAN_DECLARE(joystick_chan);
static struct joystick_data sensor_data;

static void listener_cb(const struct zbus_channel *chan)
{
    const struct joystick_data *data = zbus_chan_const_msg(chan);
    sensor_data.x = data->x;
    sensor_data.y = data->y;
}
ZBUS_LISTENER_DEFINE(joystick_listener, listener_cb);
ZBUS_CHAN_ADD_OBS(joystick_chan, joystick_listener, 3);

static struct http_resource_detail_static my_res_detail = {
    .common = {
        .type = HTTP_RESOURCE_TYPE_STATIC,
        .bitmask_of_supported_http_methods = BIT(HTTP_GET),
        .content_type = "text/html",
    },
    .static_data = index_html,
    .static_data_len = sizeof(index_html),
};

static int sensor_cb(struct http_client_ctx *client,
                     enum http_transaction_status status,
                     const struct http_request_ctx *request_ctx,
                     struct http_response_ctx *response_ctx,
                     void *user_data)
{
    static char json_buf[256];
    static bool headers_sent;

    if (status == HTTP_SERVER_TRANSACTION_COMPLETE ||
        status == HTTP_SERVER_TRANSACTION_ABORTED) {
        headers_sent = false;
        return 0;
    }

    if (status == HTTP_SERVER_REQUEST_DATA_FINAL) {
        snprintf(json_buf, sizeof(json_buf),
                "{\"device\":\"dinhnamuet\",\"X-axis\":%d,\"Y-axis\":%d,\"uptime secs\":%lld}",
                sensor_data.x, sensor_data.y, k_uptime_get() / 1000);

        response_ctx->body = json_buf;
        response_ctx->body_len = strlen(json_buf);
        response_ctx->final_chunk = true;

        if (!headers_sent) {
            response_ctx->status = HTTP_200_OK;
            headers_sent = true;
        }
    }

    return 0;
}

static struct http_resource_detail_dynamic sensor_detail = {
    .common = {
        .type = HTTP_RESOURCE_TYPE_DYNAMIC,
        .bitmask_of_supported_http_methods = BIT(HTTP_GET) | BIT(HTTP_POST),
        .content_type = "application/json",
    },
    .cb = sensor_cb,
};

static uint16_t http_port = 80;
HTTP_SERVICE_DEFINE(my_http_service, NULL, &http_port,
                    CONFIG_HTTP_SERVER_MAX_CLIENTS, 10, NULL, NULL, NULL);

HTTP_RESOURCE_DEFINE(my_resource, my_http_service, "/", &my_res_detail);
HTTP_RESOURCE_DEFINE(sensor_res, my_http_service, "/api/sensor", &sensor_detail);
