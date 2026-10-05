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
#include "temp_service.h"
#include "cli.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "MAIN";

static void hardware_sensor_setup(void)
{
    const gpio_num_t ow_pins[MAX_CHANNELS] = {
        CONFIG_ONEWIRE_CH0_PIN,
        CONFIG_ONEWIRE_CH1_PIN,
        CONFIG_ONEWIRE_CH2_PIN,
        CONFIG_ONEWIRE_CH3_PIN};
    temp_service_init(ow_pins);

    for (int ch = 0; ch < MAX_CHANNELS; ch++)
    {
        channel_metrics_t metrics;
        if (system_state_get_metrics(ch, &metrics) == ESP_OK)
        {
            temp_service_auto_assign(ch, metrics.temp_sensors);
            system_state_set_metrics(ch, &metrics);
            system_state_update_sensor_limits(ch);
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Battery Tester Project...");

    // Ініціалізація системи зберігання nvs
    if (nvs_flash_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize system nvs!");
        return; // Зупиняємо виконання, якщо критичний компонент не стартував
    }

    // Ініціалізація глобального стану системи (м'ютекси та масив даних)
    if (system_state_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize system state!");
        return; // Зупиняємо виконання, якщо критичний компонент не стартував
    }

    // Ініціалізація апаратної частини 1-Wire
    hardware_sensor_setup();

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
    cli_init();

    ESP_LOGI(TAG, "System initialization complete. Entering main loop.");

    // Основний цикл (тимчасова заглушка, щоб таска не завершувалася)
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}