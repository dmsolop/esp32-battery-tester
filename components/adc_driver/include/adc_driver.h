#pragma once

#include "esp_err.h"
#include <stdint.h>

// Ініціалізація шини I2C та драйвера АЦП
esp_err_t adc_driver_init(void);

// Зчитування напруги (у мікровольтах) для заданого каналу
esp_err_t adc_driver_read_voltage(uint8_t channel, uint32_t *voltage_uv);

// Зчитування струму (у мікроамперах) для заданого каналу
esp_err_t adc_driver_read_current(uint8_t channel, uint32_t *current_ua);

// Встановлення напруги для тестування каналу
void adc_driver_set_mock_voltage(uint8_t channel, uint32_t voltage_uv);

// Встановлення струму для тестування каналу
void adc_driver_set_mock_current(uint8_t channel, uint32_t current_ua);