#include "fans.h"

#include <driver/ledc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>

#include "led.h"
#include "util.h"

#define TAG "fans"

#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES LEDC_TIMER_8_BIT
#define LEDC_FREQUENCY (24000)

#define LEDC_FAN1_CHANNEL (LEDC_CHANNEL_0)
#define LEDC_FAN2_CHANNEL (LEDC_CHANNEL_1)
#define LEDC_FAN3_CHANNEL (LEDC_CHANNEL_2)
#define LEDC_FAN4_CHANNEL (LEDC_CHANNEL_3)
#define LEDC_FAN5_CHANNEL (LEDC_CHANNEL_4)

#define GPIO_FAN1_PWM (10)
#define GPIO_FAN2_PWM (12)
#define GPIO_FAN3_PWM (14)
#define GPIO_FAN4_PWM (16)
#define GPIO_FAN5_PWM (38)
#define GPIO_12V_EN (21)

static fans_pwm8_t s_state;
static SemaphoreHandle_t s_mutex;

static esp_err_t channel_config(ledc_channel_t channel, int gpio_num)
{
    ledc_channel_config_t config = {
        .speed_mode = LEDC_MODE,
        .channel = channel,
        .timer_sel = LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = gpio_num,
        .duty = 0xff, // Set duty to 100% (inverted)
        .hpoint = 0
    };
    return ledc_channel_config(&config);
}

static bool fans_persist_channel_unsafe(ledc_channel_t channel, fan_pwm8_t duty)
{
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, channel, 0xff - duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, channel));
    return duty > 0;
}

static void fans_persist_unsafe(void)
{
    bool any = false;

    any |= fans_persist_channel_unsafe(LEDC_FAN1_CHANNEL, s_state[0]);
    any |= fans_persist_channel_unsafe(LEDC_FAN2_CHANNEL, s_state[1]);
    any |= fans_persist_channel_unsafe(LEDC_FAN3_CHANNEL, s_state[2]);
    any |= fans_persist_channel_unsafe(LEDC_FAN4_CHANNEL, s_state[3]);
    any |= fans_persist_channel_unsafe(LEDC_FAN5_CHANNEL, s_state[4]);

    gpio_set_level(GPIO_12V_EN, any);

    // TODO Temporary emotes until controller is written
    if (any) {
        led_set_color((rgb_t) {
            .r = 0x00,
            .g = 0x10,
            .b = 0x00,
        });
    } else {
        led_set_color((rgb_t) {
            .r = 0x10,
            .g = 0x00,
            .b = 0x00,
        });
    }
}

esp_err_t fans_init(void)
{
    s_mutex = xSemaphoreCreateMutex();

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << GPIO_12V_EN),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);

    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = LEDC_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ESP_ERROR_CHECK(channel_config(LEDC_FAN1_CHANNEL, GPIO_FAN1_PWM));
    ESP_ERROR_CHECK(channel_config(LEDC_FAN2_CHANNEL, GPIO_FAN2_PWM));
    ESP_ERROR_CHECK(channel_config(LEDC_FAN3_CHANNEL, GPIO_FAN3_PWM));
    ESP_ERROR_CHECK(channel_config(LEDC_FAN4_CHANNEL, GPIO_FAN4_PWM));
    ESP_ERROR_CHECK(channel_config(LEDC_FAN5_CHANNEL, GPIO_FAN5_PWM));

    fans_persist_unsafe();

    return ESP_OK;
}

esp_err_t fans_command(uint8_t fan_i, fan_pwm8_t duty)
{
    if (fan_i > ARRAY_SIZE(s_state)) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_state[fan_i] = duty;
    fans_persist_unsafe();
    xSemaphoreGive(s_mutex);

    return ESP_OK;
}

esp_err_t fans_fetch(fans_pwm8_t duty_out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    memcpy(duty_out, s_state, sizeof(s_state));
    xSemaphoreGive(s_mutex);

    return ESP_OK;
}
