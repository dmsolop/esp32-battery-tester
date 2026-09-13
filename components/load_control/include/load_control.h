#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Ініціалізація та запуск тасок PID-регуляторів для всіх каналів
esp_err_t load_control_init(void);

// Отримання хендла таски конкретного каналу (для safety_monitor)
TaskHandle_t load_control_get_task_handle(uint8_t channel);