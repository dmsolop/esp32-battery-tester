#include "telemetry.h"
#include "system_state.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdio.h>

#ifndef CONFIG_MAX_CHANNELS
#define CONFIG_MAX_CHANNELS 4
#endif

static const char *TAG = "TELEMETRY";

static void telemetry_task(void *pvParameters)
{
    channel_metrics_t metrics;
    char json_buffer[256];

    while (1)
    {
        // Заглушка для відправки JSON пакета
        ESP_LOGI(TAG, "--- Sending Telemetry ---");
        for (int i = 0; i < CONFIG_MAX_CHANNELS; i++)
        {
            if (system_state_get_metrics(i, &metrics) == ESP_OK)
            {
                // Формуємо простий JSON-рядок для кожного каналу
                snprintf(json_buffer, sizeof(json_buffer),
                         "{\"ch\":%d,\"state\":%d,\"v_uv\":%lu,\"i_ua\":%lu,\"mah\":%lu,\"mwh\":%lu}",
                         i, metrics.state, metrics.voltage_uv, metrics.current_ua,
                         metrics.capacity_mah, metrics.energy_mwh);

                // Тут у майбутньому буде відправка через MQTT або WebSocket
                ESP_LOGI(TAG, "JSON: %s", json_buffer);
            }
        }
        ESP_LOGI(TAG, "-------------------------");

        // Телеметрію зазвичай відправляють рідше, ніж оновлюють UI (наприклад, раз на 5 секунд)
        vTaskDelay(pdMS_TO_TICKS(15000));
    }
}

esp_err_t telemetry_init(void)
{
    // Пріоритет найнижчий (нижчий за UI), оскільки це не критичний для роботи процес
    BaseType_t res = xTaskCreate(telemetry_task, "telemetry_task", 4096, NULL, configMAX_PRIORITIES - 4, NULL);
    if (res != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create telemetry task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Telemetry service initialized");
    return ESP_OK;
}