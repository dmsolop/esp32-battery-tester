#include "safety_monitor.h"
#include "system_state.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "load_control.h"
#include "ds18b20.h" // Підключення нашого нового драйвера

#ifndef CONFIG_MAX_CHANNELS
#define CONFIG_MAX_CHANNELS 4
#endif

// Макрос для піна, якщо він не заданий через Kconfig
#ifndef CONFIG_ONEWIRE_PIN
#define CONFIG_ONEWIRE_PIN GPIO_NUM_4
#endif

// Жорсткі ліміти безпеки
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

    // Змінні для неблокуючого опитування температури
    bool temp_conversion_started = false;
    TickType_t last_temp_request_time = 0;

    while (1)
    {
        TickType_t current_time = xTaskGetTickCount();

        // 1. Асинхронний запит на вимірювання (Convert T) кожні 1000 мс
        if (!temp_conversion_started && (current_time - last_temp_request_time) >= pdMS_TO_TICKS(1000))
        {
            for (int i = 0; i < CONFIG_MAX_CHANNELS; i++)
            {
                if (system_state_get_metrics(i, &metrics) == ESP_OK)
                {
                    for (int s = 0; s < MAX_SENSORS_PER_CHANNEL; s++)
                    {
                        if (metrics.temp_sensors[s].is_bound)
                        {
                            ds18b20_request_temperature(CONFIG_ONEWIRE_PIN, metrics.temp_sensors[s].rom);
                        }
                    }
                }
            }
            temp_conversion_started = true;
            last_temp_request_time = current_time;
        }

        // 2. Зчитування температури після завершення конвертації (через 750 мс)
        if (temp_conversion_started && (current_time - last_temp_request_time) >= pdMS_TO_TICKS(750))
        {
            for (int i = 0; i < CONFIG_MAX_CHANNELS; i++)
            {
                if (system_state_get_metrics(i, &metrics) == ESP_OK)
                {
                    bool metrics_updated = false;
                    for (int s = 0; s < MAX_SENSORS_PER_CHANNEL; s++)
                    {
                        if (metrics.temp_sensors[s].is_bound)
                        {
                            int32_t temp_mc = 0;
                            if (ds18b20_read_temperature(CONFIG_ONEWIRE_PIN, metrics.temp_sensors[s].rom, &temp_mc))
                            {
                                metrics.temp_sensors[s].current_temp_mc = temp_mc;
                                metrics_updated = true;
                            }
                        }
                    }
                    if (metrics_updated)
                    {
                        system_state_set_metrics(i, &metrics);
                    }
                }
            }
            temp_conversion_started = false;
        }

        // 3. Основний цикл швидких перевірок хард-лімітів (10 Гц)
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

                        // Перевірка електричних хард-лімітів
                        if (metrics.current_ua > MAX_CURRENT_UA)
                            errors |= ERR_OVER_CURRENT;
                        if (metrics.voltage_uv > MAX_VOLTAGE_UV)
                            errors |= ERR_OVER_VOLTAGE;

                        // Перевірка OTP для кожного індивідуального датчика (рольова модель)
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

                        // Якщо виявлено порушення лімітів
                        if (errors != 0)
                        {
                            ESP_LOGE(TAG, "CRITICAL ERROR on CH%d! Mask: 0x%02lX. Emergency stop!", i, errors);

                            // 1. Оновлюємо стан у загальній структурі
                            metrics.error_flags = errors;
                            metrics.state = STATE_ERROR;
                            system_state_set_metrics(i, &metrics);

                            // 2. Виклик xTaskNotifyGive() для передачі Direct Notification
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