#include "temperature.h"

#include <string.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <driver/i2c.h>
#include <driver/shtc3.h>

#include "i2c_bus.h"

#define TAG "temperature"

static SemaphoreHandle_t s_mutex;
static shtc3_sample_t s_sample;
static bool s_sample_valid;

static void temperature_task(void *arg)
{
    while (1)
    {
        shtc3_sample_t sample;
        if (shtc3_measure(I2C_BUS_EXTERNAL_NUM, &sample) == ESP_OK)
        {
            xSemaphoreTake(s_mutex, portMAX_DELAY);
            memcpy(&s_sample, &sample, sizeof(s_sample));
            s_sample_valid = true;
            xSemaphoreGive(s_mutex);
        }
        else
        {
            xSemaphoreTake(s_mutex, portMAX_DELAY);
            s_sample_valid = false;
            xSemaphoreGive(s_mutex);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

bool temperature_fetch(temperature_sample_t *sample_out)
{
    bool sample_valid;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    sample_valid = s_sample_valid;
    if (sample_valid)
    {
        sample_out->temperature_mc = s_sample.temperature_mc;
        sample_out->rel_hum_mperct = s_sample.rel_hum_mperct;
    }
    xSemaphoreGive(s_mutex);

    return sample_valid;
}

esp_err_t temperature_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    s_sample_valid = false;

    xTaskCreate(temperature_task, "temperature", 1024 * 4, NULL, 10, NULL);

    return ESP_OK;
}