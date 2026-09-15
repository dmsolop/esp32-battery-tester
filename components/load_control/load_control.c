#include "load_control.h"
#include "system_state.h"
#include "adc_driver.h"
#include "esp_log.h"
#include "esp_timer.h"

#ifndef CONFIG_MAX_CHANNELS
#define CONFIG_MAX_CHANNELS 4
#endif

static const char *TAG = "LOAD_CTRL";
static TaskHandle_t s_pid_tasks[CONFIG_MAX_CHANNELS] = {NULL};

// Тимчасова заглушка для апаратного керування MOSFET (ШІМ / ЦАП)
static void hw_set_load_pwm(uint8_t channel, uint32_t duty)
{
    if (duty == 0)
    {
        // Гарантоване закриття транзистора (наприклад, ledc_set_duty(..., 0))
        // ESP_LOGD(TAG, "CH%d: MOSFET OFF", channel);
    }
    else
    {
        // Встановлення робочого ШІМ
        // ESP_LOGD(TAG, "CH%d: MOSFET PWM = %lu", channel, duty);
    }
}

// Функція таски для окремого каналу
static void pid_control_task(void *pvParameters)
{
    uint8_t channel = (uint8_t)((uint32_t)pvParameters);
    channel_metrics_t metrics;

    int64_t last_time_us = esp_timer_get_time();

    while (1)
    {
        // Очікуємо повідомлення (Direct Task Notification) з таймаутом 50 мс.
        // Це визначає частоту циклу ПІД-регулятора (20 Гц).
        uint32_t notification = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50));

        system_state_get_metrics(channel, &metrics);

        // Реакція на сигнал аварії від Task_Safety
        if (notification > 0)
        {
            ESP_LOGW(TAG, "CH%d: Emergency Abort received! Cutting power.", channel);
            hw_set_load_pwm(channel, 0); // МИТТЄВЕ апаратне відключення

            metrics.state = STATE_ERROR;
            system_state_set_metrics(channel, &metrics);
            continue; // Пропускаємо решту циклу, чекаємо наступної ітерації
        }

        int64_t current_time_us = esp_timer_get_time();
        int64_t dt_us = current_time_us - last_time_us;
        last_time_us = current_time_us;

        // Обробка кінцевого автомата
        switch (metrics.state)
        {
        case STATE_IDLE:
        case STATE_FINISHED:
        case STATE_ERROR:
            // У цих станах навантаження має бути гарантовано вимкнене
            hw_set_load_pwm(channel, 0);
            break;

        case STATE_PRE_CHECK:
            // Навантаження ще вимкнене, але ми маємо зчитати напругу розімкнутого кола (Vocv)
            hw_set_load_pwm(channel, 0);
            adc_driver_read_voltage(channel, &metrics.voltage_uv);

            // Записуємо Vocv в систему, щоб UI або інша логіка могли прийняти рішення про старт
            system_state_set_metrics(channel, &metrics);
            break;

        case STATE_DISCHARGING:
            // 1. Зчитування реальних (або замоканих) даних з АЦП
            adc_driver_read_voltage(channel, &metrics.voltage_uv);
            adc_driver_read_current(channel, &metrics.current_ua);

            // 2. TODO: Розрахунок PID і оновлення ШІМ
            // uint32_t calc_duty = pid_compute(...);
            // hw_set_load_pwm(channel, calc_duty);

            // 3. Інтегрування ємності та енергії
            // Заряд = Струм (мкА) * Час (с) -> мкА*с
            metrics.accumulated_uas += (metrics.current_ua * dt_us) / 1000000;

            // Енергія = Напруга (мВ) * Струм (мА) * Час (с) -> мкВт*с
            uint64_t power_uw = (metrics.voltage_uv / 1000) * (metrics.current_ua / 1000);
            metrics.accumulated_uws += (power_uw * dt_us) / 1000000;

            // Конвертація для відображення в UI
            metrics.capacity_mah = metrics.accumulated_uas / 3600;
            metrics.energy_mwh = metrics.accumulated_uws / 3600;

            // Збереження розрахованих даних у загальний стан
            system_state_set_metrics(channel, &metrics);
            break;

        default:
            break;
        }
    }
}

esp_err_t load_control_init(void)
{
    for (uint32_t i = 0; i < CONFIG_MAX_CHANNELS; i++)
    {
        char task_name[16];
        snprintf(task_name, sizeof(task_name), "pid_task_%lu", i);

        BaseType_t res = xTaskCreate(pid_control_task, task_name, 4096, (void *)i, configMAX_PRIORITIES - 2, &s_pid_tasks[i]);
        if (res != pdPASS)
        {
            ESP_LOGE(TAG, "Failed to create %s", task_name);
            return ESP_FAIL;
        }
    }

    ESP_LOGI(TAG, "Load control initialized (%d tasks)", CONFIG_MAX_CHANNELS);
    return ESP_OK;
}

TaskHandle_t load_control_get_task_handle(uint8_t channel)
{
    if (channel >= CONFIG_MAX_CHANNELS)
        return NULL;
    return s_pid_tasks[channel];
}