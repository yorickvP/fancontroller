#include "tacho.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <driver/pulse_cnt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include "util.h"

#define TAG "tacho"

#define GPIO_FAN1_TACHO (11)
#define GPIO_FAN2_TACHO (13)
#define GPIO_FAN3_TACHO (15)
#define GPIO_FAN4_TACHO (17)
#define GPIO_FAN5_TACHO (39)

#define TACHO_DELTA_GLITCH_FILTER_NS 1000 // 1us
#define TACHO_HIGH_LIMIT 5000 // 5000 rotations per 500ms = 150000 RPM

typedef struct
{
  uint64_t last_time;
  uint32_t cur_rpm;
  pcnt_unit_handle_t pcnt_unit;
} tacho_fan_state_t;

static tacho_fan_state_t s_tacho_fan_state[5];
static SemaphoreHandle_t s_mutex;


static void tacho_start()
{
    gpio_config_t io_conf = {
      //.intr_type = GPIO_INTR_POSEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << GPIO_FAN1_TACHO) | (1ULL << GPIO_FAN2_TACHO) | (1ULL << GPIO_FAN3_TACHO) | (1ULL << GPIO_FAN4_TACHO) | (1ULL << GPIO_FAN5_TACHO),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);
    const pcnt_unit_config_t unit_config = {
      .low_limit = -1,
      .high_limit = TACHO_HIGH_LIMIT,
    };
    const pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = TACHO_DELTA_GLITCH_FILTER_NS,
    };
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const uint8_t gpios[] = {
      GPIO_FAN1_TACHO,
      GPIO_FAN2_TACHO,
      GPIO_FAN3_TACHO,
      GPIO_FAN4_TACHO,
      GPIO_FAN5_TACHO
    };
    for (uint8_t i = 0; i < 4; i++) {
      pcnt_chan_config_t chan_config = {
        .edge_gpio_num = gpios[i],
        .level_gpio_num = -1
      };
      pcnt_unit_handle_t pcnt_unit = NULL;
      ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit));
      pcnt_channel_handle_t pcnt_chan = NULL;
      ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_config, &pcnt_chan));
      ESP_ERROR_CHECK(pcnt_channel_set_edge_action(
                                                   pcnt_chan,
                                                   PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                                                   PCNT_CHANNEL_EDGE_ACTION_INCREASE));
      ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan,
                                                    PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                                    PCNT_CHANNEL_LEVEL_ACTION_KEEP));
      ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit, &filter_config));
      ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit));

      ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));

      ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit));
      s_tacho_fan_state[i].last_time = esp_timer_get_time();
      s_tacho_fan_state[i].cur_rpm = 0;
      s_tacho_fan_state[i].pcnt_unit = pcnt_unit;
    }
    xSemaphoreGive(s_mutex);
}

static void tacho_task(void* arg)
{

    tacho_start();
    while (1) {
      vTaskDelay(1000 / portTICK_PERIOD_MS);

      xSemaphoreTake(s_mutex, portMAX_DELAY);

      for (uint8_t i = 0; i < 4; i++) {
        pcnt_unit_handle_t pcnt_unit = s_tacho_fan_state[i].pcnt_unit;
        pcnt_unit_stop(pcnt_unit);
        uint64_t time_passed = esp_timer_get_time() - s_tacho_fan_state[i].last_time;
        int count;
        ESP_ERROR_CHECK(pcnt_unit_get_count(pcnt_unit, &count));
        ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
        // 4: empirically determined
        s_tacho_fan_state[i].cur_rpm = (uint32_t)((uint64_t)count * 60000000 / time_passed) / 4;
        s_tacho_fan_state[i].last_time = esp_timer_get_time();
        ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit));
      }
      // TODO: fifth fan
      s_tacho_fan_state[5].cur_rpm = 0;
      xSemaphoreGive(s_mutex);
    }
}

esp_err_t tacho_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    xTaskCreate(tacho_task, "tacho", 1024 * 4, NULL, 10, NULL);

    return ESP_OK;
}

void tacho_fetch(tacho_fans_rpm_t fans)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    for (size_t i = 0; i < ARRAY_SIZE(s_tacho_fan_state); ++i) {
      fans[i] = s_tacho_fan_state[i].cur_rpm;
    }

    xSemaphoreGive(s_mutex);
}
