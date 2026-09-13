#include "ui_interface.h"
#include "system_state.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#ifndef CONFIG_MAX_CHANNELS
#define CONFIG_MAX_CHANNELS 4
#endif

static const char *TAG = "UI";

static void ui_task(void *pvParameters)
{
    channel_metrics_t metrics;

    while (1)
    {
        // Заглушка: виведення дашборду в консоль замість OLED
        for (int i = 0; i < CONFIG_MAX_CHANNELS; i++)
        {
            if (system_state_get_metrics(i, &metrics) == ESP_OK)
            {
                // Виводимо лише базові метрики для контролю
                ESP_LOGI(TAG, "CH%d | State: %d | V: %lu uV | I: %lu uA | Cap: %lu mAh | E: %lu mWh",
                         i, metrics.state, metrics.voltage_uv, metrics.current_ua,
                         metrics.capacity_mah, metrics.energy_mwh);
            }
            else
            {
                ESP_LOGE(TAG, "CH%d | Failed to read metrics!", i);
            }
        }

        // Оновлюємо інформацію раз на 2 секунди, щоб не спамити лог
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

esp_err_t ui_interface_init(void)
{
    // Пріоритет середній (нижчий за safety та PID)
    BaseType_t res = xTaskCreate(ui_task, "ui_task", 4096, NULL, configMAX_PRIORITIES - 3, NULL);
    if (res != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create UI task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "UI Interface initialized");
    return ESP_OK;
}