#include <zephyr/kernel.h>
#include <zephyr/net/http/service.h>
#include <zephyr/logging/log.h>

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
    "<p><a href=\"/api/status\">Nothing</a></p>"
    "</div></body></html>"
};

static struct http_resource_detail_static my_res_detail = {
    .common = {
        .type = HTTP_RESOURCE_TYPE_STATIC,
        .bitmask_of_supported_http_methods = BIT(HTTP_GET),
        .content_type = "text/html",
    },
    .static_data = index_html,
    .static_data_len = sizeof(index_html),
};

static uint16_t http_port = 80;
HTTP_SERVICE_DEFINE(my_http_service, NULL, &http_port,
                    CONFIG_HTTP_SERVER_MAX_CLIENTS, 10, NULL, NULL, NULL);

HTTP_RESOURCE_DEFINE(my_resource, my_http_service, "/", &my_res_detail);
