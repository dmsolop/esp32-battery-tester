#pragma once

#include "system_types.h"
#include "driver/gpio.h"
#include <stdint.h>

// Максимальна кількість датчиків на одній фізичній шині OneWire
#define MAX_SYSTEM_SENSORS 8

// Ініціалізація шини та сканування адрес датчиків
void temp_service_init(gpio_num_t onewire_pin);

// Асинхронний автомат для зчитування температури без блокування процесора
// Оновлює переданий масив датчиків та повертає кількість активних пристроїв
void temp_service_process(temp_sensor_data_t *system_sensors, uint8_t *out_count);