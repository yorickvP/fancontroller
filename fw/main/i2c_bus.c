#include "i2c_bus.h"

#define TAG "i2c_bus"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <driver/i2c.h>

#define I2C_BUS_PRIMARY_SCL_IO (37)
#define I2C_BUS_PRIMARY_SDA_IO (36)

#define I2C_BUS_EXTERNAL_SCL_IO (42)
#define I2C_BUS_EXTERNAL_SDA_IO (41)

#define I2C_MASTER_FREQ_HZ 400000
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0

static SemaphoreHandle_t s_mutex[SOC_I2C_NUM];

esp_err_t i2c_bus_init(void)
{
    for (size_t i = 0; i < SOC_I2C_NUM; ++i)
    {
        s_mutex[i] = xSemaphoreCreateMutex();
    }

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_BUS_PRIMARY_SDA_IO,
        .scl_io_num = I2C_BUS_PRIMARY_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    i2c_param_config(I2C_BUS_PRIMARY_NUM, &conf);
    i2c_driver_install(I2C_BUS_PRIMARY_NUM, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);

    conf = (i2c_config_t){
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_BUS_EXTERNAL_SDA_IO,
        .scl_io_num = I2C_BUS_EXTERNAL_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    i2c_param_config(I2C_BUS_EXTERNAL_NUM, &conf);
    i2c_driver_install(I2C_BUS_EXTERNAL_NUM, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);

    return ESP_OK;
}

esp_err_t i2c_bus_take(i2c_port_t port)
{
    return (xSemaphoreTake(s_mutex[port], portMAX_DELAY) == pdTRUE) ? ESP_OK : ESP_ERR_INVALID_STATE;
}

esp_err_t i2c_bus_give(i2c_port_t port)
{
    return (xSemaphoreGive(s_mutex[port]) == pdTRUE) ? ESP_OK : ESP_ERR_INVALID_STATE;
}