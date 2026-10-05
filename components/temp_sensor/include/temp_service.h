#pragma once

#include "system_types.h"
#include "driver/gpio.h"
#include <stdint.h>

#pragma once

#include "system_types.h"
#include "driver/gpio.h"
#include <stdint.h>
#include <stdbool.h>

// Ініціалізує 4 незалежні шини OneWire
void temp_service_init(const gpio_num_t pins[4]);

// Автоматичне сканування фізичної шини каналу та зчитування ролей з EEPROM (байт T_L)
void temp_service_auto_assign(uint8_t channel, temp_sensor_data_t *sensors);

// Апаратний броадкаст: запускає конвертацію ОДНОЧАСНО на всіх 4 шинах
bool temp_service_trigger_conversion(void);
bool temp_service_is_conversion_done(void);

// Зчитування результатів для конкретного каналу
void temp_service_read_sensors(uint8_t channel, temp_sensor_data_t *sensors);