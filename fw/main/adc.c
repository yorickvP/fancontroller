#include "adc.h"

#include <string.h>
#include <math.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <esp_adc/adc_continuous.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>

#include "events.h"
#include "util.h"

#define TAG "adc"

#define ADC_BIT_WIDTH (SOC_ADC_DIGI_MAX_BITWIDTH)
#define ADC_UNIT ADC_UNIT_1

#define ADC_ATTEN ADC_ATTEN_DB_0 /* Up to 750mV */
#define ADC_RAW_RANGE (1 << ADC_BIT_WIDTH)

#define CURRENT_SENSE_RES_MILLIOHM 30
#define CURRENT_SENSE_AMPLIFICATION 20

#define ADC_CHANNEL(adc_channel_nr) ADC_CHANNEL_##adc_channel_nr

static adc_channel_t adc_channel[8] = {
    ADC_CHANNEL(0),
    ADC_CHANNEL(1),
    ADC_CHANNEL(3),
    ADC_CHANNEL(4),
    ADC_CHANNEL(5),
    ADC_CHANNEL(6),
    ADC_CHANNEL(7),
    ADC_CHANNEL(8),
};

#define ADC_OVERSAMPLING 32
#define ADC_FRAME_LEN (SOC_ADC_DIGI_DATA_BYTES_PER_CONV * ARRAY_SIZE(adc_channel) * ADC_OVERSAMPLING)

#define SAMPLES_COUNT ARRAY_SIZE(adc_channel)

typedef struct
{
    uint32_t min;
    uint32_t max;
    uint32_t rms;
    uint32_t count;
} sample_intermediate_t;

static TaskHandle_t s_task_handle;
static SemaphoreHandle_t s_mutex;

static adc_cali_handle_t s_cali_handle;
static adc_samples_t s_samples;

static uint32_t min(uint32_t x, uint32_t y)
{
    return (x < y) ? x : y;
}

static uint32_t max(uint32_t x, uint32_t y)
{
    return (y < x) ? x : y;
}

static uint32_t raw_to_millivolts(uint32_t raw)
{
    int voltage;
    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(s_cali_handle, raw, &voltage));
    return voltage;
}

static adc_sample_t sample_voltage_divider(adc_sample_t sample, uint32_t r1, uint32_t r2)
{
    sample.rms = ((uint32_t)sample.rms) * (r1 + r2) / r2;
    sample.max = ((uint32_t)sample.max) * (r1 + r2) / r2;
    return sample;
}

static adc_sample_t sample_current_sense(adc_sample_t sample)
{
    sample.rms = ((uint32_t)sample.rms) * 1000 / CURRENT_SENSE_RES_MILLIOHM / CURRENT_SENSE_AMPLIFICATION;
    sample.max = ((uint32_t)sample.max) * 1000 / CURRENT_SENSE_RES_MILLIOHM / CURRENT_SENSE_AMPLIFICATION;
    return sample;
}

static adc_sample_t sample_from_intermediate(sample_intermediate_t sample_intermediate)
{
    uint16_t rms = sqrt(sample_intermediate.rms / sample_intermediate.count);

    return (adc_sample_t){
        .max = sample_intermediate.max,
        .rms = rms,
    };
}

typedef sample_intermediate_t samples_intermediate_t[SAMPLES_COUNT];

static void samples_from_intermediate(const samples_intermediate_t sample_intermediate, adc_samples_t *samples)
{
    samples->vbus_mv = sample_voltage_divider(sample_from_intermediate(sample_intermediate[0]), 1000, 47);
    samples->vfan_mv = sample_voltage_divider(sample_from_intermediate(sample_intermediate[1]), 1000, 47);
    samples->vbus_ma = sample_current_sense(sample_from_intermediate(sample_intermediate[2]));

    for (int i = 0; i < 5; i++)
    {
        samples->vfan_ma[i] = sample_current_sense(sample_from_intermediate(sample_intermediate[3 + i]));
    }
}

static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
{
    BaseType_t must_yield = pdFALSE;
    // Notify that ADC continuous driver has done enough number of conversions
    vTaskNotifyGiveFromISR(s_task_handle, &must_yield);

    return (must_yield == pdTRUE);
}

static void continuous_adc_init(adc_continuous_handle_t *out_handle)
{
    adc_continuous_handle_t handle = NULL;

    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 1024,
        .conv_frame_size = ADC_FRAME_LEN,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = 10000,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
    };

    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};

    dig_cfg.pattern_num = ARRAY_SIZE(adc_channel);
    for (int i = 0; i < ARRAY_SIZE(adc_channel); i++)
    {
        adc_pattern[i] = (adc_digi_pattern_config_t){
            .atten = ADC_ATTEN,
            .channel = adc_channel[i],
            .unit = ADC_UNIT,
            .bit_width = ADC_BIT_WIDTH,
        };
    }
    dig_cfg.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));

    *out_handle = handle;
}

static void adc_task(void *arg)
{
    uint32_t ret_num = 0;
    uint8_t result[ADC_FRAME_LEN] = {0};
    memset(result, 0xcc, ADC_FRAME_LEN);

    s_task_handle = xTaskGetCurrentTaskHandle();

    adc_continuous_handle_t handle = NULL;
    continuous_adc_init(&handle);

    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = s_conv_done_cb,
    };
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &cbs, NULL));

    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT,
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BIT_WIDTH,
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &s_cali_handle));

    ESP_ERROR_CHECK(adc_continuous_start(handle));

    uint8_t channel_map[SOC_ADC_PATT_LEN_MAX] = {0xff};
    sample_intermediate_t samples[ARRAY_SIZE(adc_channel)];

    for (uint8_t i = 0; i < ARRAY_SIZE(adc_channel); i++)
    {
        channel_map[adc_channel[i]] = i;
    }

    while (1)
    {
        /**
         * This is to show you the way to use the ADC continuous mode driver event callback.
         * This `ulTaskNotifyTake` will block when the data processing in the task is fast.
         * However in this example, the data processing (print) is slow, so you barely block here.
         *
         * Without using this event callback (to notify this task), you can still just call
         * `adc_continuous_read()` here in a loop, with/without a certain block timeout.
         */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        while (1)
        {
            for (int i = 0; i < ARRAY_SIZE(samples); ++i)
            {
                samples[i] = (sample_intermediate_t){
                    .rms = 0,
                    .min = 0xffffffff,
                    .max = 0,
                    .count = 0,
                };
            }

            esp_err_t ret = adc_continuous_read(handle, result, ADC_FRAME_LEN, &ret_num, 0);
            if (ret == ESP_OK)
            {
                for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES)
                {
                    adc_digi_output_data_t *p = (void *)&result[i];
                    uint32_t chan_num = p->type2.channel;
                    uint32_t data = p->type2.data;
                    /* Check the channel number validation, the data is invalid if the channel num exceed the maximum channel */
                    if (chan_num < SOC_ADC_CHANNEL_NUM(ADC_UNIT))
                    {
                        // ESP_LOGI(TAG, "Channel: %" PRIu32 ", Value: %" PRIx32 ", Real: %lu", chan_num, data, data * 750 / 4095);

                        uint8_t channel_i = channel_map[chan_num];
                        if (channel_i < ARRAY_SIZE(samples))
                        {
                            data = raw_to_millivolts(data);

                            samples[channel_i].rms += data * data;
                            samples[channel_i].min = min(samples[channel_i].min, data);
                            samples[channel_i].max = max(samples[channel_i].max, data);
                            samples[channel_i].count++;
                        }
                        else
                        {
                            ESP_LOGW(TAG, "Invalid mapped channel");
                        }
                    }
                    else
                    {
                        ESP_LOGW(TAG, "Invalid data [%" PRIu32 "_%" PRIx32 "]", chan_num, data);
                    }
                }
                /**
                 * Because printing is slow, so every time you call `ulTaskNotifyTake`, it will immediately return.
                 * To avoid a task watchdog timeout, add a delay here. When you replace the way you process the data,
                 * usually you don't need this delay (as this task will block for a while).
                 */
                vTaskDelay(1);

                xSemaphoreTake(s_mutex, portMAX_DELAY);
                samples_from_intermediate(samples, &s_samples);
                xSemaphoreGive(s_mutex);

                ESP_ERROR_CHECK(esp_event_post(EVENTS, EVENT_ADC_SAMPLED,
                                       NULL, 0, portMAX_DELAY));

                // for (int i = 0; i < ARRAY_SIZE(samples); ++i)
                // {
                //     uint32_t reading_rms = sqrt(samples[i].rms / samples[i].count);
                //     ESP_LOGI(TAG, "Reading: %i: rms %lumV, min %lumV, max %lumV [%lu]", i, reading_rms, samples[i].min, samples[i].max, samples[i].count);
                // }
            }
            else if (ret == ESP_ERR_TIMEOUT)
            {
                // We try to read `FRAME_READ_LEN` until API returns timeout, which means there's no available data
                break;
            }
        }
    }
}

esp_err_t adc_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    xTaskCreate(adc_task, "adc", 1024 * 4, (void *)0, 9, NULL);

    return ESP_OK;
}

void adc_fetch(adc_samples_t *samples)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    memcpy(samples, &s_samples, sizeof(s_samples));
    xSemaphoreGive(s_mutex);
}