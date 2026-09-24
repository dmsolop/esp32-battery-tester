#pragma once

#include "system_types.h"
#include "driver/gpio.h"
#include <stdint.h>

// Ініціалізує шину OneWire
void temp_service_init(gpio_num_t onewire_pin);

// Допоміжні функції для Task_UI: сканують шину та повертають знайдені ROM-адреси для їх подальшої прив'язки
// Новий розділений API кінцевого автомата
bool temp_service_trigger_conversion(void);
bool temp_service_is_conversion_done(void);
void temp_service_read_sensors(temp_sensor_data_t *sensors, uint8_t count);

// Асинхронний автомат. Приймає масив датчиків зі структури метрик і оновлює лише current_temp_mc
void temp_service_process(temp_sensor_data_t *sensors, uint8_t count);