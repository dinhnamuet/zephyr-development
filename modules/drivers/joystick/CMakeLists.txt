# SPDX-License-Identifier: Apache-2.0

if(CONFIG_JOYSTICK)
    zephyr_library()
    zephyr_library_sources(src/joystick.c)
    zephyr_library_include_directories(inc)
    zephyr_include_directories(inc)
endif()
