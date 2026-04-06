#include <zephyr/kernel.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/kvss/nvs.h>

#include "storage.h"

LOG_MODULE_REGISTER(data_storage);

#if defined(CONFIG_DT_HAS_ESPRESSIF_ESP32_FLASH_CONTROLLER_ENABLED)
#define NVS_PARTITION           storage_partition
#else
#define NVS_PARTITION           my_storage
#endif
#define NVS_PARTITION_DEVICE    PARTITION_DEVICE(NVS_PARTITION)
#define NVS_PARTITION_OFFSET    PARTITION_OFFSET(NVS_PARTITION)
#define NVS_PARTITION_SIZE      PARTITION_SIZE(NVS_PARTITION)

static struct nvs_fs fs;

int storage_init(void)
{
    int ret;
    struct flash_pages_info info;

    fs.flash_device = NVS_PARTITION_DEVICE;
    if (!device_is_ready(fs.flash_device)) {
        LOG_ERR("Storage device %s not found!", fs.flash_device->name);
        return -ENODEV;
    }
    fs.offset = NVS_PARTITION_OFFSET;
    ret = flash_get_page_info_by_offs(fs.flash_device, fs.offset, &info);
    if (ret) {
        LOG_ERR("Unable to get page info, ret: %d", ret);
        return ret;
    }
    fs.sector_size = info.size;
    fs.sector_count = NVS_PARTITION_SIZE / fs.sector_size;

    ret = nvs_mount(&fs);
    if (ret) {
        LOG_ERR("NVS mount failed, ret: %d", ret);
        return ret;
    }
    LOG_INF("NVS Mounted at %ld, sector size: %d, sector count: %d", fs.offset, fs.sector_size, fs.sector_count);
    return ret;
}

ssize_t storage_read(uint16_t id, void *data, size_t len)
{
    return nvs_read(&fs, id, data, len);
}

ssize_t storage_write(uint16_t id, const void *data, size_t len)
{
    return nvs_write(&fs, id, data, len);
}
