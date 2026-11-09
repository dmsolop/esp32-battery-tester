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
// Підтримує до 3 датчиків (2 x SENSOR_ROLE_HEATSINK + 1 x SENSOR_ROLE_CELL) з фолбеком
void temp_service_auto_assign(uint8_t channel, temp_sensor_data_t *sensors);

// Програмна зміна ролі датчика з записом у EEPROM (Scratchpad + Copy Scratchpad)
// та оновленням лімітів у system_state
esp_err_t temp_service_bind_sensor_role(uint8_t channel, uint8_t sensor_idx, sensor_role_t new_role);

// Примусове ручне пересканування шини OneWire та оновлення прив'язок у system_state
esp_err_t temp_service_rescan(uint8_t channel);

// Апаратний броадкаст: запускає конвертацію ОДНОЧАСНО на всіх 4 шинах
bool temp_service_trigger_conversion(void);
bool temp_service_is_conversion_done(void);

// Зчитування результатів для конкретного каналу
void temp_service_read_sensors(uint8_t channel, temp_sensor_data_t *sensors);