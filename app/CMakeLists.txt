# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(
    dinhnamuet_zephyr
    LANGUAGES C
)

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/inc)

zephyr_library_sources_ifdef(CONFIG_JOYSTICK src/joystick_hal.c)
zephyr_library_sources_ifdef(CONFIG_WATCHDOG src/wdt.c)
zephyr_library_sources_ifdef(CONFIG_USB_CUSTOM_CLASS src/usb.c)
zephyr_library_sources_ifdef(CONFIG_USB_CUSTOM_CLASS src/dfu.c)
zephyr_library_sources_ifdef(CONFIG_NVS src/storage.c)
zephyr_library_sources_ifdef(CONFIG_UART_ASYNC_API src/serial.c)
zephyr_library_sources_ifdef(CONFIG_COUNTER src/counter.c)
zephyr_library_sources_ifdef(CONFIG_SENSOR src/tsensor.c)
zephyr_library_sources_ifdef(CONFIG_NET_TCP src/tcp_server.c src/dfu.c)
zephyr_library_sources_ifdef(CONFIG_HTTP_SERVER src/http_server.c)
zephyr_library_sources_ifdef(CONFIG_UPDATEHUB src/updatehub_ota.c)

zephyr_linker_sources(SECTIONS sections-rom.ld)
zephyr_linker_section(NAME http_resource_desc_my_http_service
                      KVMA RAM_REGION GROUP RODATA_REGION)

target_sources(app PRIVATE main.c)
