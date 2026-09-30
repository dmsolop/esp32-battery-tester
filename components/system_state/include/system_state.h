#pragma once

#include "esp_err.h"
#include "system_types.h"

// Ініціалізація внутрішнього м'ютекса та обнулення масиву каналів
esp_err_t system_state_init(void);

// Потокобезпечний запис та читання повної структури метрик для заданого каналу
esp_err_t system_state_set_metrics(uint8_t channel, const channel_metrics_t *metrics);
esp_err_t system_state_get_metrics(uint8_t channel, channel_metrics_t *out_metrics);

// Швидкий потокобезпечний доступ виключно до стану автомата
esp_err_t system_state_set_channel_state(uint8_t channel, channel_state_t state);
esp_err_t system_state_get_channel_state(uint8_t channel, channel_state_t *out_state);

// Встановлення хімії з автоматичним застосуванням дефолтних налаштувань та збереженням у NVS
esp_err_t system_state_set_chemistry(uint8_t channel, battery_chem_t chem);

// Примусове оновлення лімітів для всіх прив'язаних датчиків каналу
esp_err_t system_state_update_sensor_limits(uint8_t channel);