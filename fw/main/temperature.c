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

#define GPIO_EXT_INT (40)

typedef struct
{
    SemaphoreHandle_t mutex;
    shtc3_sample_t sample;
    bool sample_valid;
} temperature_sensor_t;

static temperature_sensor_t s_sensors[TEMPERATURE_CHANNEL_MAX_COUNT];

static esp_err_t temperature_measure(i2c_port_t port, shtc3_sample_t* sample)
{
    esp_err_t ret;

    ERROR_CHECK_SIMPLE(i2c_bus_take(port));
    ret = shtc3_measure(port, sample);
    i2c_bus_give(port); // Ignore error
err:
    return ret;
}

static gpio_num_t temperature_get_presence_gpio(temperature_channel_t channel)
{
    switch (channel) {
    case TEMPERATURE_CHANNEL_EXTERNAL:
        return GPIO_EXT_INT;
    default:
        return GPIO_NUM_NC;
    }
}

static bool temperature_is_present_unsafe(temperature_channel_t channel)
{
    const gpio_num_t gpio_presence = temperature_get_presence_gpio(channel);

    if (gpio_presence == GPIO_NUM_NC) {
        return true; // Having no detection GPIO implies that it is always present.
    }

    // Strobe GPIO pin pull down in case it is not connected (and hence not pulled to a known state)
    gpio_set_pull_mode(gpio_presence, GPIO_PULLDOWN_ONLY);
    vTaskDelay(1); // Wait for one tick.
    gpio_set_pull_mode(gpio_presence, GPIO_FLOATING);
    vTaskDelay(1); // Wait for one tick.

    return (gpio_get_level(gpio_presence) == 1);
}

static void temperature_measure_store_unsafe(temperature_channel_t channel, i2c_port_t port)
{
    shtc3_sample_t sample;
    temperature_sensor_t* sensor = &s_sensors[channel];

    bool is_present = temperature_is_present_unsafe(channel);

    if (is_present && temperature_measure(port, &sample) == ESP_OK) {
        xSemaphoreTake(sensor->mutex, portMAX_DELAY);
        memcpy(&sensor->sample, &sample, sizeof(sample));
        sensor->sample_valid = true;
        xSemaphoreGive(sensor->mutex);
    } else {
        xSemaphoreTake(sensor->mutex, portMAX_DELAY);
        sensor->sample_valid = false;
        xSemaphoreGive(sensor->mutex);
    }
}

static void temperature_task(void* arg)
{
    while (1) {
        temperature_measure_store_unsafe(TEMPERATURE_CHANNEL_ON_BOARD, I2C_BUS_PRIMARY_NUM);
        temperature_measure_store_unsafe(TEMPERATURE_CHANNEL_EXTERNAL, I2C_BUS_EXTERNAL_NUM);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

bool temperature_fetch(temperature_channel_t channel, temperature_sample_t* sample_out)
{
    bool sample_valid;
    temperature_sensor_t* sensor = &s_sensors[channel];

    xSemaphoreTake(sensor->mutex, portMAX_DELAY);
    sample_valid = sensor->sample_valid;
    if (sample_valid) {
        sample_out->temperature_mc = sensor->sample.temperature_mc;
        sample_out->rel_hum_mperct = sensor->sample.rel_hum_mperct;
    }
    xSemaphoreGive(sensor->mutex);

    return sample_valid;
}

esp_err_t temperature_init(void)
{
    for (temperature_channel_t channel = 0; channel < TEMPERATURE_CHANNEL_MAX_COUNT; ++channel) {
        temperature_sensor_t* sensor = &s_sensors[channel];
        sensor->mutex = xSemaphoreCreateMutex();
        sensor->sample_valid = false;

        gpio_num_t gpio_presence = temperature_get_presence_gpio(channel);

        if (gpio_presence != GPIO_NUM_NC) {
            gpio_config_t io_conf = {
                .intr_type = GPIO_INTR_DISABLE,
                .mode = GPIO_MODE_INPUT,
                .pin_bit_mask = (1ULL << gpio_presence),
                .pull_down_en = 0,
                .pull_up_en = 0,
            };
            gpio_config(&io_conf);
        }
    }

    xTaskCreate(temperature_task, "temperature", 1024 * 4, NULL, 10, NULL);

    return ESP_OK;
}