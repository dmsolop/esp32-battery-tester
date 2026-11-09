#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Ініціалізація та запуск тасок PID-регуляторів для всіх каналів
esp_err_t load_control_init(void);

// Отримання хендла таски конкретного каналу (для safety_monitor)
TaskHandle_t load_control_get_task_handle(uint8_t channel);

// Пряме апаратне керування ШІМ навантаження (duty: 0..4095)
// Дозволяє виставити опорну напругу V_REF для лабораторних випробувань та калібрування
void hw_set_load_pwm(uint8_t channel, uint32_t duty);

// Встановлення ручного тестового ШІМ з фіксацією прапорця manual_override.
// duty > 0: активує manual_override = true і виставляє ШІМ;
// duty == 0: виставляє ШІМ 0 і вимикає manual_override = false.
void load_control_set_raw_pwm(uint8_t channel, uint16_t duty);

// Негайна зупинка розряду каналу: примусове обнулення ШІМ, скидання стану в STATE_IDLE
void load_control_stop(uint8_t channel);

// Запуск одиночного діагностичного Pre-Check OCV (замір OCV, перевірка напруги та повернення в STATE_IDLE без розряду)
void load_control_start_diag_precheck(uint8_t channel);

// Запуск одиночного діагностичного HW Self-Test (перевірка термодатчиків, нуля та 100 мА імпульсу з поверненням в STATE_IDLE)
void load_control_start_diag_self_test(uint8_t channel);

// Запуск одиночного вимірювання DCIR (імпульс 100 мА на 100 мс, розрахунок dcir_mohm та миттєве повернення в STATE_IDLE)
void load_control_trigger_dcir_pulse(uint8_t channel);

// Керування ШІМ охолоджувального кулера (duty_percent: 0..100%).
// duty_percent > 0: вмикає ручне перевизначення (manual override).
// Якщо manual override скинуто (або duty_percent == 0 при явному вимкненні),
// кулер може регулюватися автоматично за термопрофілем.
void load_control_set_fan_pwm(uint8_t channel, uint8_t duty_percent);
uint8_t load_control_get_fan_pwm(uint8_t channel);
void load_control_set_fan_manual_override(uint8_t channel, bool manual);