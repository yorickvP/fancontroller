#pragma once

#include <esp_err.h>

#include <driver/i2c.h>

#define I2C_BUS_PRIMARY_NUM 0
#define I2C_BUS_EXTERNAL_NUM 1

esp_err_t i2c_bus_init(void);

esp_err_t i2c_bus_take(i2c_port_t port);
esp_err_t i2c_bus_give(i2c_port_t port);
