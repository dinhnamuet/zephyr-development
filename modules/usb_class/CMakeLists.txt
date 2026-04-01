# SPDX-License-Identifier: Apache-2.0

if(CONFIG_USB_CUSTOM_CLASS)
    zephyr_library()
    zephyr_library_sources(src/custom_class.c)
    zephyr_library_include_directories(inc)
    zephyr_include_directories(inc)
endif()
