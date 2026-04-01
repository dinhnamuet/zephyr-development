#pragma once

struct sensor_data {
    uint32_t time_stamp_seconds;
    double temperature;
};

int tsensor_polling_start(const uint32_t polling_interval_ms);
