#pragma once

#include <esp_err.h>

typedef struct
{
    uint16_t max;
    uint16_t rms;
} adc_sample_t;

typedef struct
{
    adc_sample_t vbus_mv;
    adc_sample_t vfan_mv;
    adc_sample_t vbus_ma;
    adc_sample_t vfan_ma[5];
} adc_samples_t;

esp_err_t adc_init(void);

void adc_fetch(adc_samples_t *samples);
