#pragma once

#include <esp_err.h>

typedef uint32_t tacho_fan_rpm_t;
typedef tacho_fan_rpm_t tacho_fans_rpm_t[5];

esp_err_t tacho_init(void);

void tacho_fetch(tacho_fans_rpm_t fans);
