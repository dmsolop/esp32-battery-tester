#include "cli.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "CLI";

static void cli_task(void *arg)
{
    char line[128];
    ESP_LOGI(TAG, "CLI started. Type: start <ch>, stop <ch>, fault <ch>, set_v <ch> <uV>");

    while (1)
    {
        // Читаємо рядок зі стандартного вводу (UART)
        if (fgets(line, sizeof(line), stdin) != NULL)
        {
            // Видаляємо символи переносу рядка
            line[strcspn(line, "\r\n")] = 0;
            if (strlen(line) == 0)
                continue;

            char cmd[16];
            int ch = -1;
            uint32_t val = 0;

            // Парсимо рядок: очікуємо рядок, ціле число, і (опціонально) ще одне число
            int parsed = sscanf(line, "%15s %d %lu", cmd, &ch, &val);

            if (parsed >= 2 && ch >= 0 && ch < 4)
            {
                if (strcmp(cmd, "start") == 0)
                {
                    ESP_LOGI(TAG, "=> Command received: START channel %d", ch);
                    // TODO: system_state_send_command(ch, CMD_START);
                }
                else if (strcmp(cmd, "stop") == 0)
                {
                    ESP_LOGI(TAG, "=> Command received: STOP channel %d", ch);
                    // TODO: system_state_send_command(ch, CMD_STOP);
                }
                else if (strcmp(cmd, "fault") == 0)
                {
                    ESP_LOGI(TAG, "=> Command received: FAULT channel %d", ch);
                    // TODO: trigger_safety_fault(ch);
                }
                else if (strcmp(cmd, "set_v") == 0 && parsed == 3)
                {
                    ESP_LOGI(TAG, "=> Command received: SET_V channel %d to %lu uV", ch, val);
                    // TODO: adc_driver_set_mock_voltage(ch, val);
                }
                else
                {
                    ESP_LOGW(TAG, "Unknown command: %s", cmd);
                }
            }
            else
            {
                ESP_LOGW(TAG, "Invalid format. Use: <cmd> <ch> [val]");
            }
        }
        // Невелика затримка, щоб не блокувати процесор (хоча fgets блокує сам)
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

esp_err_t cli_init(void)
{
    // Створюємо таску з низьким пріоритетом (це лише інтерфейс)
    if (xTaskCreate(cli_task, "cli_task", 4096, NULL, 2, NULL) != pdPASS)
    {
        return ESP_FAIL;
    }
    return ESP_OK;
}