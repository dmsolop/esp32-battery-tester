#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"

// Підключаємо наш новий компонент
#include "safety_monitor.h"
#include "system_state.h"
#include "load_control.h"
#include "ui_interface.h"
#include "telemetry.h"
#include "adc_driver.h"

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

    // Ініціалізація монітора безпеки (КРИТИЧНО)
    if (safety_monitor_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize safety monitor!");
        return; // Безпека понад усе, зупиняємось!
    }

    // Ініціалізація драйвера шини I2C
    if (adc_driver_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize adc driver!");
        return;
    }

    // Ініціалізація контролю навантаження (КРИТИЧНО)
    if (load_control_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize load control!");
        return; // Без регуляторів прилад не має сенсу
    }

    ui_interface_init();
    telemetry_init();

    ESP_LOGI(TAG, "System initialization complete. Entering main loop.");

    // Основний цикл (тимчасова заглушка, щоб таска не завершувалася)
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}