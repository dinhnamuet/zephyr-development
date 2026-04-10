#!/bin/sh
BUILD=$(git rev-list --count HEAD)
VERSION=1.0.2
VERSION_SIGN="\"${VERSION}+${BUILD}\""

west build -p always -b doit_esp32_devkit_v1/esp32/procpu \
  /home/nam/zephyr-development/vendor/dinhnamuet/app -- \
  -DCONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION=${VERSION_SIGN} \
  -DDTC_OVERLAY_FILE="/home/nam/zephyr-development/vendor/dinhnamuet/app/boards/esp32/doit_esp32_devkit_v1_procpu.overlay" \
  -DEXTRA_CONF_FILE="/home/nam/zephyr-development/vendor/dinhnamuet/app/conf/esp32/esp32.conf" 2>&1 | tee build.log

rm .uhu
rm artifacts/*
uhu product use "e4d37cfe6ec48a2d069cc0bbb8b078677e9a0d8df3a027c4d8ea131130c4265f"
uhu package add build/zephyr/zephyr.signed.bin -m zephyr
uhu package version ${VERSION}
uhu package archive --output artifacts/zephyr-${VERSION}.pkg
