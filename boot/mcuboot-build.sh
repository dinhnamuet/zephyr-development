#! /bin/sh
west build -p auto -b nucleo_h743zi \
  /home/nam/zephyr-development/bootloader/mcuboot/boot/zephyr \
  -- \
  -DDTC_OVERLAY_FILE="/home/nam/zephyr-development/vendor/dinhnamuet/boot/boards/nucleo_h743zi.overlay" \
  -DEXTRA_CONF_FILE="/home/nam/zephyr-development/vendor/dinhnamuet/boot/boot.conf" 2>&1 | tee build.log
