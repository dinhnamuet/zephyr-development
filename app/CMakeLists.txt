# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(
    dinhnamuet_zephyr
    LANGUAGES C
)

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/inc)

file(GLOB APP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

target_sources(
    app
    PRIVATE
    main.c
    ${APP_SOURCES}
)
