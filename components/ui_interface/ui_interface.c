#include "ui_interface.h"
#include "display_engine.h"
#include "input_service.h"
#include "icons.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

typedef enum
{
    UI_STATE_CH_LIST,
    UI_STATE_PARAM_LIST,
    UI_STATE_GRAPH
} ui_state_t;

static void ui_render_dashboard(uint8_t selected_ch, uint8_t selected_param, ui_state_t state)
{
    display_engine_clear();

    // --- ЛІВИЙ ЕКРАН ---
    display_engine_draw_bitmap(0, 4, icon_wifi_8x8, 8, 8, 1);
    display_engine_draw_string(12, 4, "12:51  READY", 1);

    for (int i = 0; i < 4; i++)
    {
        int y_pos = 18 + (i * 11);
        char ch_str[20];
        snprintf(ch_str, sizeof(ch_str), "CH%d: 3.7%dV 1.00A", i, i);

        if (state == UI_STATE_CH_LIST && i == selected_ch)
        {
            display_engine_draw_string(0, y_pos, ">", 1);
        }
        display_engine_draw_string(8, y_pos, ch_str, 1);
    }

    // --- ПРАВИЙ ЕКРАН ---
    int rx = 128;

    if (state == UI_STATE_GRAPH)
    {
        display_engine_draw_string(rx + 4, 4, "--- GRAPH VIEW ---", 1);
        display_engine_draw_string(rx + 10, 30, "[ GRAPH RENDERING ]", 1);
        display_engine_draw_string(rx + 20, 50, "Click to Exit", 1);
    }
    else
    {
        char title_str[24];
        snprintf(title_str, sizeof(title_str), "--- CH %d DETAIL ---", selected_ch);
        display_engine_draw_string(rx + 4, 4, title_str, 1);

        const char *params[] = {"MODE: DISCHARGE", "CAP:  1250 mAh", "RES:  45 mOhm", "[ <- BACK ]"};

        for (int i = 0; i < 4; i++)
        {
            int y_pos = 20 + (i * 12);
            if (state == UI_STATE_PARAM_LIST && i == selected_param)
            {
                display_engine_draw_string(rx + 0, y_pos, ">", 1);
            }
            display_engine_draw_string(rx + 8, y_pos, params[i], 1);
        }
    }
}

static void ui_task(void *pvParameters)
{
    ui_state_t current_state = UI_STATE_CH_LIST;
    int selected_ch = 0;
    int selected_param = 0;

    while (1)
    {
        int diff = 0;
        bool btn_clicked = false;

        // Делегуємо зчитування апаратури сервісу
        input_service_read(&diff, &btn_clicked);

        // 1. Обробка обертання
        if (diff != 0)
        {
            if (current_state == UI_STATE_CH_LIST)
            {
                selected_ch = (selected_ch + diff) % 4;
                if (selected_ch < 0)
                    selected_ch += 4;
            }
            else if (current_state == UI_STATE_PARAM_LIST)
            {
                selected_param = (selected_param + diff) % 4;
                if (selected_param < 0)
                    selected_param += 4;
            }
        }

        // 2. Обробка кліку
        if (btn_clicked)
        {
            if (current_state == UI_STATE_CH_LIST)
            {
                current_state = UI_STATE_PARAM_LIST;
                selected_param = 0;
            }
            else if (current_state == UI_STATE_PARAM_LIST)
            {
                if (selected_param == 3)
                {
                    current_state = UI_STATE_CH_LIST;
                }
                else
                {
                    current_state = UI_STATE_GRAPH;
                }
            }
            else if (current_state == UI_STATE_GRAPH)
            {
                current_state = UI_STATE_PARAM_LIST;
            }
        }

        // 3. Відмальовування
        ui_render_dashboard(selected_ch, selected_param, current_state);
        display_engine_update();

        vTaskDelay(pdMS_TO_TICKS(50)); // 20 FPS
    }
}

esp_err_t ui_interface_init(void)
{
    ESP_ERROR_CHECK(display_engine_init());
    ESP_ERROR_CHECK(input_service_init());

    xTaskCreate(ui_task, "ui_task", 4096, NULL, configMAX_PRIORITIES - 3, NULL);
    return ESP_OK;
}