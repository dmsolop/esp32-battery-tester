#include "ui_interface.h"
#include "display_engine.h"
#include "icons.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

static uint8_t s_selected_channel = 0;

static void ui_render_dashboard(uint8_t selected_ch)
{
    display_engine_clear();

    // --- ЛІВИЙ ЕКРАН ---
    // Малюємо значок WiFi (x=0, y=4) замість тексту
    display_engine_draw_bitmap(0, 4, icon_wifi_8x8, 8, 8, 1);

    // Текст статусу відсуваємо вправо, залишаючи місце для іконки
    display_engine_draw_string(12, 4, "12:51  READY", 1);

    for (int i = 0; i < 4; i++)
    {
        int y_pos = 18 + (i * 11);
        char ch_str[20];
        snprintf(ch_str, sizeof(ch_str), "CH%d: 3.7%dV 1.00A", i, i);

        if (i == selected_ch)
            display_engine_draw_string(0, y_pos, ">", 1);
        display_engine_draw_string(8, y_pos, ch_str, 1);
    }

    // --- ПРАВИЙ ЕКРАН ---
    int rx = 128;
    char title_str[24];
    snprintf(title_str, sizeof(title_str), "--- CH %d DETAIL ---", selected_ch);

    display_engine_draw_string(rx + 4, 4, title_str, 1);
    display_engine_draw_string(rx + 0, 20, "MODE: DISCHARGE", 1);
    display_engine_draw_string(rx + 0, 32, "CAP:  1250 mAh", 1);
    display_engine_draw_string(rx + 0, 44, "RES:  45 mOhm", 1);
    display_engine_draw_string(rx + 0, 56, "TIME: 01:23:45", 1);
}

static void ui_task(void *pvParameters)
{
    while (1)
    {
        ui_render_dashboard(s_selected_channel);
        display_engine_update();

        s_selected_channel++;
        if (s_selected_channel >= 4)
            s_selected_channel = 0;

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

esp_err_t ui_interface_init(void)
{
    // Тепер UI модуль лише делегує ініціалізацію екранів рушію
    ESP_ERROR_CHECK(display_engine_init());

    // Тут у майбутньому буде: encoder_init(), buzzer_init()...

    xTaskCreate(ui_task, "ui_task", 4096, NULL, configMAX_PRIORITIES - 3, NULL);
    return ESP_OK;
}