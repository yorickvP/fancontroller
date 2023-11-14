#include "tacho.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_timer.h>
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

#define TACHO_DELTA_GLITCH_FILTER_US 1000 // 1 millisecond or 60000RPM
#define TACHO_READING_MAX_AGE_US 1000000 // 1 seconds or 60RPM

typedef struct
{
    uint8_t gpio_i;
    uint64_t time;
} tacho_event_t;

typedef struct
{
    uint64_t delta;
    uint64_t last_time;
} tacho_fan_state_t;

static tacho_fan_state_t s_tacho_fan_state[5];
static QueueHandle_t s_event_queue = NULL;
static SemaphoreHandle_t s_mutex;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    tacho_event_t event = {
        .gpio_i = (uint32_t)arg,
        .time = esp_timer_get_time(),
    };

    xQueueSendFromISR(s_event_queue, &event, NULL);
}

static void tacho_task(void* arg)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_POSEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << GPIO_FAN1_TACHO) | (1ULL << GPIO_FAN2_TACHO) | (1ULL << GPIO_FAN3_TACHO) | (1ULL << GPIO_FAN4_TACHO) | (1ULL << GPIO_FAN5_TACHO),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(GPIO_FAN1_TACHO, gpio_isr_handler, (void*)0);
    gpio_isr_handler_add(GPIO_FAN2_TACHO, gpio_isr_handler, (void*)1);
    gpio_isr_handler_add(GPIO_FAN3_TACHO, gpio_isr_handler, (void*)2);
    gpio_isr_handler_add(GPIO_FAN4_TACHO, gpio_isr_handler, (void*)3);
    gpio_isr_handler_add(GPIO_FAN5_TACHO, gpio_isr_handler, (void*)4);

    tacho_event_t event;
    while (1) {
        if (xQueueReceive(s_event_queue, &event, portMAX_DELAY)) {
            tacho_fan_state_t* state = &s_tacho_fan_state[event.gpio_i];
            uint64_t delta = event.time - state->last_time;

            if (delta >= TACHO_DELTA_GLITCH_FILTER_US) // Glitch filter
            {
                xSemaphoreTake(s_mutex, portMAX_DELAY);
                state->delta = delta;
                state->last_time = event.time;
                xSemaphoreGive(s_mutex);
            }
        }
    }
}

esp_err_t tacho_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    s_event_queue = xQueueCreate(10, sizeof(tacho_event_t));
    xTaskCreate(tacho_task, "tacho", 1024 * 4, NULL, 10, NULL);

    return ESP_OK;
}

void tacho_fetch(tacho_fans_rpm_t fans)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    uint64_t now = esp_timer_get_time();

    for (size_t i = 0; i < ARRAY_SIZE(s_tacho_fan_state); ++i) {
        uint32_t rpm = 0;
        if (s_tacho_fan_state[i].delta > 0 && s_tacho_fan_state[i].last_time + TACHO_READING_MAX_AGE_US > now) {
            rpm = (60 * 1000 * 1000) / s_tacho_fan_state[i].delta; // us to RPM
        }
        fans[i] = rpm;
    }
    xSemaphoreGive(s_mutex);
}