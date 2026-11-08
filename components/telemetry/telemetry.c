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
    char json_buffer[384];

    while (1)
    {
        // Заглушка для відправки JSON пакета
        ESP_LOGI(TAG, "--- Sending Telemetry ---");
        for (int i = 0; i < CONFIG_MAX_CHANNELS; i++)
        {
            if (system_state_get_metrics(i, &metrics) == ESP_OK)
            {
                // Отримуємо температури радіаторів (до 2) та комірки (до 1)
                int32_t t_hs1_mc = 0;
                int32_t t_hs2_mc = 0;
                int32_t t_cell_mc = 0;
                bool hs1_found = false;

                for (int s = 0; s < MAX_SENSORS_PER_CHANNEL; s++)
                {
                    if (metrics.temp_sensors[s].is_bound)
                    {
                        if (metrics.temp_sensors[s].role == SENSOR_ROLE_HEATSINK)
                        {
                            if (!hs1_found)
                            {
                                t_hs1_mc = metrics.temp_sensors[s].current_temp_mc;
                                hs1_found = true;
                            }
                            else
                            {
                                t_hs2_mc = metrics.temp_sensors[s].current_temp_mc;
                            }
                        }
                        else if (metrics.temp_sensors[s].role == SENSOR_ROLE_CELL)
                        {
                            t_cell_mc = metrics.temp_sensors[s].current_temp_mc;
                        }
                    }
                }

                // Формуємо розширений JSON-рядок для кожного каналу з температурами у міліградусах
                snprintf(json_buffer, sizeof(json_buffer),
                         "{\"ch\":%d,\"state\":%d,\"v_uv\":%lu,\"i_ua\":%lu,\"mah\":%lu,\"mwh\":%lu,\"t_hs1_mc\":%ld,\"t_hs2_mc\":%ld,\"t_cell_mc\":%ld}",
                         i, metrics.state, (unsigned long)metrics.voltage_uv, (unsigned long)metrics.current_ua,
                         (unsigned long)metrics.capacity_mah, (unsigned long)metrics.energy_mwh,
                         (long)t_hs1_mc, (long)t_hs2_mc, (long)t_cell_mc);

                // Тут у майбутньому буде відправка через MQTT або WebSocket
                ESP_LOGI(TAG, "JSON: %s", json_buffer);
            }
        }
        ESP_LOGI(TAG, "-------------------------");

        // Телеметрію зазвичай відправляють рідше, ніж оновлюють UI (наприклад, раз на 15 секунд)
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