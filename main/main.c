#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"

// Підключаємо наш новий компонент
#include "system_state.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Battery Tester Project...");

    // Ініціалізація глобального стану системи (м'ютекси та масив даних)
    esp_err_t err = system_state_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize system state!");
        return; // Зупиняємо виконання, якщо критичний компонент не стартував
    }

    ESP_LOGI(TAG, "System initialization complete. Entering main loop.");

    // Основний цикл (тимчасова заглушка, щоб таска не завершувалася)
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}