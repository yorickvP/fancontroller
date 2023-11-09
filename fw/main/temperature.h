#pragma once

#include <esp_err.h>
#include <stdbool.h>

typedef struct
{
    int32_t temperature_mc;
    int32_t rel_hum_mperct;
} temperature_sample_t;

esp_err_t temperature_init(void);

bool temperature_fetch(temperature_sample_t *sample_out);
