#pragma once

#include "system_types.h"
#include "driver/gpio.h"
#include <stdint.h>

// Ініціалізує шину OneWire
void temp_service_init(gpio_num_t onewire_pin);

// Допоміжна функція для Task_UI: сканує шину та повертає знайдені ROM-адреси для їх подальшої прив'язки
// Повертає кількість знайдених датчиків
uint8_t temp_service_scan_bus(uint8_t discovered_roms[][8], uint8_t max_roms);

// Асинхронний автомат. Приймає масив датчиків зі структури метрик і оновлює лише current_temp_mc
void temp_service_process(temp_sensor_data_t *sensors, uint8_t count);