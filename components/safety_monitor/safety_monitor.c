#include "safety_monitor.h"
#include "system_state.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#ifndef CONFIG_MAX_CHANNELS
#define CONFIG_MAX_CHANNELS 4
#endif

// Жорсткі ліміти безпеки
#define MAX_TEMP_MCELSIUS 80000 // 80.0 °C (макс. температура радіатора)
#define MAX_CURRENT_UA 5000000  // 5.0 A (макс. струм розряду)
#define MAX_VOLTAGE_UV 20000000 // 20.0 V (макс. вхідна напруга)

// Бітові маски помилок
#define ERR_OVER_TEMP 0x01
#define ERR_OVER_CURRENT 0x02
#define ERR_OVER_VOLTAGE 0x04

static const char *TAG = "SAFETY";

static void safety_task(void *pvParameters)
{
    channel_metrics_t metrics;
    channel_state_t current_state;

    while (1)
    {
        for (int i = 0; i < CONFIG_MAX_CHANNELS; i++)
        {
            // Перевіряємо стан. Моніторинг має сенс лише в активних фазах розряду/заряду.
            if (system_state_get_channel_state(i, &current_state) == ESP_OK)
            {
                if (current_state == STATE_DISCHARGING || current_state == STATE_CHARGING)
                {

                    if (system_state_get_metrics(i, &metrics) == ESP_OK)
                    {
                        uint32_t errors = 0;

                        // Перевірка хард-лімітів
                        if (metrics.temp_mcelsius > MAX_TEMP_MCELSIUS)
                            errors |= ERR_OVER_TEMP;
                        if (metrics.current_ua > MAX_CURRENT_UA)
                            errors |= ERR_OVER_CURRENT;
                        if (metrics.voltage_uv > MAX_VOLTAGE_UV)
                            errors |= ERR_OVER_VOLTAGE;

                        // Якщо виявлено порушення лімітів
                        if (errors != 0)
                        {
                            ESP_LOGE(TAG, "CRITICAL ERROR on CH%d! Mask: 0x%02lX. Emergency stop!", i, errors);

                            // 1. Оновлюємо стан у загальній структурі
                            metrics.error_flags = errors;
                            metrics.state = STATE_ERROR;
                            system_state_set_metrics(i, &metrics);

                            // 2. TODO: Виклик xTaskNotify() для передачі Direct Notification
                            // у Task_PID_Control(i) для миттєвого апаратного відключення ШІМ.
                        }
                    }
                }
            }
        }
        // Затримка 100 мс (10 перевірок на секунду)
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

esp_err_t safety_monitor_init(void)
{
    // Створюємо таску з найвищим пріоритетом (configMAX_PRIORITIES - 1)
    BaseType_t res = xTaskCreate(safety_task, "safety_task", 3072, NULL, configMAX_PRIORITIES - 1, NULL);
    if (res != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create safety_task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Safety monitor initialized and running");
    return ESP_OK;
}