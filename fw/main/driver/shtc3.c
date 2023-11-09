#include "shtc3.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>

#include "util.h"

#define TAG "shtc3"

#define I2C_MASTER_TIMEOUT_MS 1000
#define SHTC3_SENSOR_ADDR 0x70

static inline uint16_t sys_get_be16(const uint8_t src[2])
{
    return ((uint16_t)src[0] << 8) | src[1];
}

static inline void sys_put_be16(uint16_t val, uint8_t dst[2])
{
    dst[0] = val >> 8;
    dst[1] = val;
}

// static esp_err_t shtc3_register_read_register(i2c_port_t port, uint16_t command, uint16_t *data)
// {
//     uint8_t write_buf[2];
//     sys_put_be16(command, write_buf);
//     uint8_t *read_buf = (uint8_t *)data;
//     return i2c_master_write_read_device(port, SHTC3_SENSOR_ADDR, write_buf, sizeof(write_buf), read_buf, 2, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
// }

static esp_err_t shtc3_register_read(i2c_port_t port, uint8_t *data, size_t len)
{
    return i2c_master_read_from_device(port, SHTC3_SENSOR_ADDR, data, len, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

static esp_err_t shtc3_register_write_command(i2c_port_t port, uint16_t command)
{
    int ret;
    uint8_t write_buf[2];
    sys_put_be16(command, write_buf);

    ret = i2c_master_write_to_device(port, SHTC3_SENSOR_ADDR, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    return ret;
}

esp_err_t shtc3_measure(i2c_port_t port, shtc3_sample_t *sample_out)
{
    esp_err_t ret;
    ERROR_CHECK_SIMPLE(shtc3_register_write_command(port, 0x3517));
    vTaskDelay(pdMS_TO_TICKS(20));

    ERROR_CHECK_SIMPLE(shtc3_register_write_command(port, 0x7866));

    uint8_t data[6] = {0};
    while (1)
    {
        if (ESP_OK == shtc3_register_read(port, data, sizeof(data)))
        {
            uint16_t temp_raw = sys_get_be16(&data[0]);
            uint16_t hum_raw = sys_get_be16(&data[3]);

            sample_out->temperature_mc = ((((int32_t)temp_raw) * 21875) >> 13) - 45000;
            sample_out->rel_hum_mperct = (((int32_t)hum_raw) * 12500) >> 13;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    ERROR_CHECK_SIMPLE(shtc3_register_write_command(port, 0xB098));
    return ESP_OK;
err:
    return ret;
}