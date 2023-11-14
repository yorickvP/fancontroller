#include <esp_log.h>
#include <nvs_flash.h>

#include "adc.h"
#include "events.h"
#include "fans.h"
#include "http_server.h"
#include "i2c_bus.h"
#include "led.h"
#include "mqtt.h"
#include "performance.h"
#include "periodic.h"
#include "tacho.h"
#include "temperature.h"
#include "wifi.h"

#define TAG "main"

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_ERROR_CHECK(events_init());
    ESP_ERROR_CHECK(performance_init());
    ESP_ERROR_CHECK(i2c_bus_init());

    ESP_ERROR_CHECK(led_init());
    led_set_color((rgb_t) {
        .r = 0x10,
        .g = 0x00,
        .b = 0x00,
    });

    ESP_ERROR_CHECK(adc_init());
    ESP_ERROR_CHECK(tacho_init());
    ESP_ERROR_CHECK(fans_init());

    ESP_ERROR_CHECK(temperature_init());

    ESP_ERROR_CHECK(wifi_init());
    ESP_ERROR_CHECK(http_server_init());
    ESP_ERROR_CHECK(mqtt_init());

    ESP_ERROR_CHECK(periodic_init());

    ESP_LOGI(TAG, "Device initialized, running event loop");
}
