#pragma once

#include <stdint.h>
#include "esp_err.h"

// Максимальне значення ШІМ для 12-бітної роздільної здатності (2^12 - 1)
#define PWM_MAX_DUTY 4095

// Мінімальна скважність утримання обертання кулера BLDC (~28% від 4095) для подолання заклинювання
#define FAN_MIN_RUN_DUTY 1150

// Частота низькочастотної ШІМ для 3-провідних BLDC кулерів (Sanyo Denki San Ace)
#define FAN_PWM_FREQ_HZ 30

esp_err_t load_control_pwm_init(uint8_t channel, int gpio_num);
esp_err_t load_control_pwm_set(uint8_t channel, uint32_t duty);

// Ініціалізація та керування ШІМ кулера охолодження (LEDC_CHANNEL_4..7)
esp_err_t load_control_fan_pwm_init(uint8_t channel, int gpio_num);
esp_err_t load_control_fan_pwm_set(uint8_t channel, uint32_t duty);

// ISR-безпечна функція екстреного скидання всіх каналів
void pwm_driver_emergency_stop_isr(void);