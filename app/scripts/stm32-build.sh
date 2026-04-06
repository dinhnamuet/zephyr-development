#!/bin/sh
BUILD=$(git rev-list --count HEAD)
VERSION="\"1.0.2+${BUILD}\""

west build -p auto -b nucleo_h743zi \
  /home/nam/zephyr-development/vendor/dinhnamuet/app -- \
  -DCONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION=${VERSION} \
  -DDTC_OVERLAY_FILE="/home/nam/zephyr-development/vendor/dinhnamuet/app/boards/stm32/nucleo_h743zi.overlay" \
  -DEXTRA_CONF_FILE="/home/nam/zephyr-development/vendor/dinhnamuet/app/conf/stm32/stm32.conf" 2>&1 | tee build.log
