#include "temperature.h"

#include <driver/i2c.h>
#include <driver/shtc3.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <string.h>

#include "i2c_bus.h"
#include "util.h"

#define TAG "temperature"

typedef struct
{
    SemaphoreHandle_t mutex;
    shtc3_sample_t sample;
    bool sample_valid;
} temperature_entry_t;

static temperature_entry_t s_entries[TEMPERATURE_CHANNEL_MAX_COUNT];

static esp_err_t temperature_measure(i2c_port_t port, shtc3_sample_t* sample)
{
    esp_err_t ret;

    ERROR_CHECK_SIMPLE(i2c_bus_take(port));
    ret = shtc3_measure(port, sample);
    i2c_bus_give(port); // Ignore error
err:
    return ret;
}

static void temperature_measure_store(temperature_channel_t channel, i2c_port_t port)
{
    shtc3_sample_t sample;
    temperature_entry_t* entry = &s_entries[channel];

    if (temperature_measure(port, &sample) == ESP_OK) {
        xSemaphoreTake(entry->mutex, portMAX_DELAY);
        memcpy(&entry->sample, &sample, sizeof(sample));
        entry->sample_valid = true;
        xSemaphoreGive(entry->mutex);
    } else {
        xSemaphoreTake(entry->mutex, portMAX_DELAY);
        entry->sample_valid = false;
        xSemaphoreGive(entry->mutex);
    }
}

static void temperature_task(void* arg)
{
    while (1) {
        temperature_measure_store(TEMPERATURE_CHANNEL_ON_BOARD, I2C_BUS_PRIMARY_NUM);
        temperature_measure_store(TEMPERATURE_CHANNEL_EXTERNAL, I2C_BUS_EXTERNAL_NUM);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

bool temperature_fetch(temperature_channel_t channel, temperature_sample_t* sample_out)
{
    bool sample_valid;
    temperature_entry_t* entry = &s_entries[channel];

    xSemaphoreTake(entry->mutex, portMAX_DELAY);
    sample_valid = entry->sample_valid;
    if (sample_valid) {
        sample_out->temperature_mc = entry->sample.temperature_mc;
        sample_out->rel_hum_mperct = entry->sample.rel_hum_mperct;
    }
    xSemaphoreGive(entry->mutex);

    return sample_valid;
}

esp_err_t temperature_init(void)
{
    for (size_t i = 0; i < TEMPERATURE_CHANNEL_MAX_COUNT; ++i) {
        temperature_entry_t* entry = &s_entries[i];
        entry->mutex = xSemaphoreCreateMutex();
        entry->sample_valid = false;
    }

    xTaskCreate(temperature_task, "temperature", 1024 * 4, NULL, 10, NULL);

    return ESP_OK;
}