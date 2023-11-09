#pragma once

#include <esp_err.h>

typedef uint8_t fan_pwm8_t;
typedef fan_pwm8_t fans_pwm8_t[5];

#define FANS_COUNT (sizeof(fans_pwm8_t) / sizeof(fan_pwm8_t))

esp_err_t fans_init(void);

esp_err_t fans_command(uint8_t fan_i, const fan_pwm8_t duty);
esp_err_t fans_fetch(fans_pwm8_t duty_out);
