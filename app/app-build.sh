#!/bin/sh
BUILD=$(git rev-list --count HEAD)
VERSION="\"1.0.2+${BUILD}\""

west build -p auto -b nucleo_h743zi . -- \
  -DCONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION=${VERSION} 2>&1 | tee build.log
