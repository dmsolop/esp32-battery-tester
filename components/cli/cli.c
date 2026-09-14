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
    int pos = 0;

    ESP_LOGI(TAG, "CLI started. Type: start <ch>, stop <ch>, fault <ch>, set_v <ch> <uV>");

    while (1)
    {
        int c = fgetc(stdin);

        if (c == EOF)
        {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        // Відлуння: відправляємо символ назад у термінал
        fputc(c, stdout);
        fflush(stdout);

        if (c == '\n' || c == '\r')
        {
            // Щоб після Enter лог не злипався з нашим вводом
            fputc('\n', stdout);

            if (pos > 0)
            {
                line[pos] = '\0';

                char cmd[16];
                int ch = -1;
                uint32_t val = 0;

                int parsed = sscanf(line, "%15s %d %lu", cmd, &ch, &val);

                if (parsed >= 2 && ch >= 0 && ch < 4)
                {
                    if (strcmp(cmd, "start") == 0)
                    {
                        ESP_LOGI(TAG, "=> Command received: START channel %d", ch);
                    }
                    else if (strcmp(cmd, "stop") == 0)
                    {
                        ESP_LOGI(TAG, "=> Command received: STOP channel %d", ch);
                    }
                    else if (strcmp(cmd, "fault") == 0)
                    {
                        ESP_LOGI(TAG, "=> Command received: FAULT channel %d", ch);
                    }
                    else if (strcmp(cmd, "set_v") == 0 && parsed == 3)
                    {
                        ESP_LOGI(TAG, "=> Command received: SET_V channel %d to %lu uV", ch, val);
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

                pos = 0;
            }
        }
        else if (c >= 32 && c <= 126 && pos < sizeof(line) - 1)
        {
            line[pos++] = (char)c;
        }
    }
}

esp_err_t cli_init(void)
{
    if (xTaskCreate(cli_task, "cli_task", 4096, NULL, 2, NULL) != pdPASS)
    {
        return ESP_FAIL;
    }
    return ESP_OK;
}