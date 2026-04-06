#!/bin/sh
BUILD=$(git rev-list --count HEAD)
VERSION="\"1.0.2+${BUILD}\""

west build -p auto -b doit_esp32_devkit_v1/esp32/procpu \
  /home/nam/zephyr-development/vendor/dinhnamuet/app -- \
  -DCONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION=${VERSION} \
  -DDTC_OVERLAY_FILE="/home/nam/zephyr-development/vendor/dinhnamuet/app/boards/esp32/doit_esp32_devkit_v1_procpu.overlay" \
  -DEXTRA_CONF_FILE="/home/nam/zephyr-development/vendor/dinhnamuet/app/conf/esp32/esp32.conf" 2>&1 | tee build.log
