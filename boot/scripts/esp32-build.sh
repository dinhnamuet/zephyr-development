#! /bin/sh
west build -p auto -b doit_esp32_devkit_v1/esp32/procpu \
  /home/nam/zephyr-development/bootloader/mcuboot/boot/zephyr \
  -- \
  -DDTC_OVERLAY_FILE="/home/nam/zephyr-development/vendor/dinhnamuet/boot/boards/esp32/doit_esp32_devkit_v1_procpu.overlay" \
  -DEXTRA_CONF_FILE="/home/nam/zephyr-development/vendor/dinhnamuet/boot/conf/esp32/esp32.conf" 2>&1 | tee build.log
