#pragma once

#include <esp_err.h>
#include <stdbool.h>

typedef struct
{
    int32_t temperature_mc;
    int32_t rel_hum_mperct;
} temperature_sample_t;

typedef enum {
    TEMPERATURE_CHANNEL_ON_BOARD = 0,
    TEMPERATURE_CHANNEL_EXTERNAL = 1,
    TEMPERATURE_CHANNEL_MAX_COUNT,
} temperature_channel_t;

esp_err_t temperature_init(void);

bool temperature_fetch(temperature_channel_t channel, temperature_sample_t* sample_out);
