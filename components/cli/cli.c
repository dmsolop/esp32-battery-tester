#include "cli.h"
#include "system_state.h"
#include "adc_driver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "CLI";

// Тимчасова заглушка для температури, поки немає драйвера DS18B20
static void temp_sensor_set_mock(uint8_t channel, int32_t temp_mc)
{
    ESP_LOGI(TAG, "Temp mock stub: CH%d = %ld mC", channel, temp_mc);
    // TODO: Підключити реальний драйвер температури, коли він буде створений
}

static void cli_task(void *arg)
{
    char line[128];
    int pos = 0;

    ESP_LOGI(TAG, "CLI started. Type: start <ch>, stop <ch>, fault <ch>, set_v/set_i/set_t <ch> <val>");

    while (1)
    {
        int c = fgetc(stdin);

        if (c == EOF)
        {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        fputc(c, stdout);
        fflush(stdout);

        if (c == '\n' || c == '\r')
        {
            fputc('\n', stdout);

            if (pos > 0)
            {
                line[pos] = '\0';

                char cmd[16];
                int ch = -1;
                long val = 0; // Змінили на long для підтримки від'ємної температури

                int parsed = sscanf(line, "%15s %d %ld", cmd, &ch, &val);

                if (parsed >= 2 && ch >= 0 && ch < 4)
                {
                    if (strcmp(cmd, "start") == 0)
                    {
                        ESP_LOGI(TAG, "=> Command received: START channel %d", ch);
                        system_state_set_channel_state(ch, STATE_PRE_CHECK);
                    }
                    else if (strcmp(cmd, "stop") == 0)
                    {
                        ESP_LOGI(TAG, "=> Command received: STOP channel %d", ch);
                        system_state_set_channel_state(ch, STATE_IDLE);
                    }
                    else if (strcmp(cmd, "fault") == 0)
                    {
                        ESP_LOGI(TAG, "=> Command received: FAULT channel %d", ch);
                        system_state_set_channel_state(ch, STATE_ERROR);
                    }
                    else if (strcmp(cmd, "set_v") == 0 && parsed == 3)
                    {
                        ESP_LOGI(TAG, "=> Command received: SET_V channel %d to %ld uV", ch, val);
                        adc_driver_set_mock_voltage(ch, (uint32_t)val);
                    }
                    else if (strcmp(cmd, "set_i") == 0 && parsed == 3)
                    {
                        ESP_LOGI(TAG, "=> Command received: SET_I channel %d to %ld uA", ch, val);
                        adc_driver_set_mock_current(ch, (uint32_t)val);
                    }
                    else if (strcmp(cmd, "set_t") == 0 && parsed == 3)
                    {
                        ESP_LOGI(TAG, "=> Command received: SET_T channel %d to %ld mC", ch, val);
                        temp_sensor_set_mock(ch, val);
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