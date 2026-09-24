#pragma once

#include <stdint.h>
#include "esp_err.h"

// Максимальне значення ШІМ для 13-бітної роздільної здатності
#define PWM_MAX_DUTY 8191

esp_err_t load_control_pwm_init(uint8_t channel, int gpio_num);
esp_err_t load_control_pwm_set(uint8_t channel, uint32_t duty);

// ISR-безпечна функція екстреного скидання всіх каналів
void pwm_driver_emergency_stop_isr(void);