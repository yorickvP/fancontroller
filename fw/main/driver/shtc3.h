#pragma once

#include <driver/i2c.h>
#include <esp_err.h>

typedef struct
{
    int32_t temperature_mc;
    int32_t rel_hum_mperct;
} shtc3_sample_t;

esp_err_t shtc3_measure(i2c_port_t port, shtc3_sample_t* sample_out);
