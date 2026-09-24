#include "safety_monitor.h"
#include "system_state.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "load_control.h"
#include "ds18b20.h"
#include "adc_driver.h"
#include "sdkconfig.h" // Додано підключення конфігурації платформи
#include "temp_service.h"

#ifndef CONFIG_MAX_CHANNELS
#define CONFIG_MAX_CHANNELS 4
#endif

// Жорсткі ліміти безпеки
#define MAX_CURRENT_UA 5000000  // 5.0 A
#define MAX_VOLTAGE_UV 20000000 // 20.0 V

// Бітові маски помилок
#define ERR_OVER_TEMP 0x01
#define ERR_OVER_CURRENT 0x02
#define ERR_OVER_VOLTAGE 0x04

static const char *TAG = "SAFETY";

static void safety_task(void *pvParameters)
{
    channel_metrics_t metrics;
    channel_state_t current_state;

    bool temp_conversion_started = false;
    TickType_t last_temp_request_time = 0;

    while (1)
    {
        TickType_t current_time = xTaskGetTickCount();

        // 1. Асинхронний запуск конвертації для всієї шини (раз на 1000 мс)
        if (!temp_conversion_started && (current_time - last_temp_request_time) >= pdMS_TO_TICKS(1000))
        {
            if (temp_service_trigger_conversion())
            {
                temp_conversion_started = true;
                last_temp_request_time = current_time;
            }
        }

        // 2. Зчитування температури після завершення глобальної конвертації
        if (temp_conversion_started && temp_service_is_conversion_done())
        {
            // Час вийшов, датчики готові. Тепер можна безпечно перебирати канали.
            for (int i = 0; i < CONFIG_MAX_CHANNELS; i++)
            {
                if (system_state_get_metrics(i, &metrics) == ESP_OK)
                {
                    // Делегуємо читання сервісу, передаючи масив датчиків конкретного каналу
                    temp_service_read_sensors(metrics.temp_sensors, MAX_SENSORS_PER_CHANNEL);
                    system_state_set_metrics(i, &metrics);
                }
            }
            temp_conversion_started = false;
        }

        // Основний цикл швидких перевірок хард-лімітів
        for (int i = 0; i < CONFIG_MAX_CHANNELS; i++)
        {
            if (system_state_get_channel_state(i, &current_state) == ESP_OK)
            {
                if (current_state == STATE_DISCHARGING || current_state == STATE_CHARGING)
                {
                    if (system_state_get_metrics(i, &metrics) == ESP_OK)
                    {
                        uint32_t errors = 0;

                        // Примусове зчитування найсвіжіших даних з I2C для підтвердження аварії
                        adc_driver_read_voltage(i, &metrics.voltage_uv);
                        adc_driver_read_current(i, &metrics.current_ua);

                        if (metrics.current_ua > MAX_CURRENT_UA)
                            errors |= ERR_OVER_CURRENT;
                        if (metrics.voltage_uv > MAX_VOLTAGE_UV)
                            errors |= ERR_OVER_VOLTAGE;

                        for (int s = 0; s < MAX_SENSORS_PER_CHANNEL; s++)
                        {
                            if (metrics.temp_sensors[s].is_bound)
                            {
                                if (metrics.temp_sensors[s].current_temp_mc >= metrics.temp_sensors[s].limit_temp_mc)
                                {
                                    errors |= ERR_OVER_TEMP;
                                }
                            }
                        }

                        if (errors != 0)
                        {
                            ESP_LOGE(TAG, "CRITICAL ERROR on CH%d! Mask: 0x%02lX. Emergency stop!", i, errors);

                            metrics.error_flags = errors;
                            metrics.state = STATE_ERROR;
                            system_state_set_metrics(i, &metrics);

                            TaskHandle_t pid_task = load_control_get_task_handle(i);
                            if (pid_task != NULL)
                            {
                                xTaskNotifyGive(pid_task);
                            }
                        }
                    }
                }
            }
        }

        // Замість сліпого vTaskDelay чекаємо на таймер АБО миттєвий сигнал від АЦП
        uint32_t notification = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
        if (notification > 0)
        {
            ESP_LOGW(TAG, "Hardware ALERT received! Fast-tracking limits check...");
        }
    }
}

esp_err_t safety_monitor_init(void)
{
    TaskHandle_t handle = NULL;
    BaseType_t res = xTaskCreate(safety_task, "safety_task", 3072, NULL, configMAX_PRIORITIES - 1, &handle);
    if (res != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create safety_task");
        return ESP_FAIL;
    }

    // Реєструємо таску в драйвері АЦП для отримання ISR-нотифікацій
    adc_driver_register_safety_task(handle);

    ESP_LOGI(TAG, "Safety monitor initialized and running");
    return ESP_OK;
}