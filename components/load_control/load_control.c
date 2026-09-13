#include "load_control.h"
#include "system_state.h"
#include "esp_log.h"
#include "esp_timer.h"

#ifndef CONFIG_MAX_CHANNELS
#define CONFIG_MAX_CHANNELS 4
#endif

static const char *TAG = "LOAD_CTRL";
static TaskHandle_t s_pid_tasks[CONFIG_MAX_CHANNELS] = {NULL};

// Функція таски для окремого каналу
static void pid_control_task(void *pvParameters)
{
    uint8_t channel = (uint8_t)((uint32_t)pvParameters);
    channel_metrics_t metrics;

    int64_t last_time_us = esp_timer_get_time();

    while (1)
    {
        // Очікуємо повідомлення (Direct Task Notification) з таймаутом 50 мс.
        // Якщо прилетить сигнал від safety_monitor (значення > 0), негайно зупиняємось.
        uint32_t notification = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50));

        system_state_get_metrics(channel, &metrics);

        if (notification > 0)
        {
            ESP_LOGW(TAG, "CH%d: Emergency Abort received!", channel);
            // TODO: Апаратне відключення ШІМ (закрити MOSFET IRLZ44N)
            metrics.state = STATE_ERROR;
            system_state_set_metrics(channel, &metrics);
            continue;
        }

        int64_t current_time_us = esp_timer_get_time();
        int64_t dt_us = current_time_us - last_time_us;
        last_time_us = current_time_us;

        // Обробка кінцевого автомата
        switch (metrics.state)
        {
        case STATE_IDLE:
            // Очікування команди. ШІМ вимкнено.
            break;

        case STATE_PRE_CHECK:
            // TODO: Зчитування V_ocv, перехід у DISCHARGING або CHARGING
            break;

        case STATE_DISCHARGING:
            // 1. TODO: Зчитування АЦП ADS1115
            // 2. TODO: Розрахунок PID і оновлення ШІМ

            // 3. Інтегрування ємності та енергії (точність до мікросекунд)
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

        case STATE_ERROR:
        case STATE_FINISHED:
            // TODO: Гарантувати, що ШІМ вимкнено
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

        // Пріоритет нижчий за safety, але вищий за UI
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