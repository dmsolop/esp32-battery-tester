#include "ui_interface.h"
#include "display_engine.h"
#include "input_service.h"
#include "system_state.h"
#include "icons.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

// --- Стани кінцевого автомата UI ---
typedef enum
{
    UI_STATE_CH_LIST,   // Лівий екран: список 4 каналів
    UI_STATE_CH_DETAIL, // Правий екран: головний екран каналу (хімія, запуск)
    UI_STATE_SETTINGS,  // Правий екран: PRO налаштування каналу
    UI_STATE_GRAPH      // Правий екран: графік (заглушка)
} ui_state_t;

// Пункти головного меню каналу
#define CH_DETAIL_ITEMS 3
static const char *s_chem_names[] = {"Li-Ion", "NiMH  ", "LiFePO4"};

// --- Хелпери для рядків стану ---
static const char *state_to_str(channel_state_t state)
{
    switch (state)
    {
    case STATE_IDLE:
        return "IDLE   ";
    case STATE_PRE_CHECK:
        return "CHECK  ";
    case STATE_DISCHARGING:
        return "DISCHG ";
    case STATE_FINISHED:
        return "DONE   ";
    case STATE_ERROR:
        return "ERROR  ";
    default:
        return "???    ";
    }
}

// --- Рендеринг лівого екрана (список каналів) ---
static void render_left(uint8_t selected_ch, ui_state_t state)
{
    display_engine_draw_bitmap(0, 4, icon_wifi_8x8, 8, 8, 1);
    display_engine_draw_string(12, 4, "BATT ANALYZER", 1);

    for (int i = 0; i < 4; i++)
    {
        int y_pos = 18 + (i * 11);
        char ch_str[24];

        channel_metrics_t m;
        if (system_state_get_metrics(i, &m) == ESP_OK)
        {
            snprintf(ch_str, sizeof(ch_str), "CH%d:%4lumV %s",
                     i,
                     (unsigned long)(m.voltage_uv / 1000),
                     state_to_str(m.state));
        }
        else
        {
            snprintf(ch_str, sizeof(ch_str), "CH%d: ---", i);
        }

        if (state == UI_STATE_CH_LIST && i == selected_ch)
        {
            display_engine_draw_string(0, y_pos, ">", 1);
        }
        display_engine_draw_string(8, y_pos, ch_str, 1);
    }
}

// --- Рендеринг правого екрана: головний екран каналу ---
static void render_ch_detail(int rx, uint8_t ch, int selected_param, ui_state_t state)
{
    char title[20];
    snprintf(title, sizeof(title), "--- CH %d ---", ch);
    display_engine_draw_string(rx + 4, 4, title, 1);

    channel_metrics_t m;
    system_state_get_metrics(ch, &m);

    char chem_str[20];
    snprintf(chem_str, sizeof(chem_str), "Chem: %s", s_chem_names[m.settings.chem]);
    display_engine_draw_string(rx + 4, 18, chem_str, 1);
    display_engine_draw_string(rx + 4, 30, "[ -> START ]", 1);
    display_engine_draw_string(rx + 4, 42, "[ SETTINGS ]", 1);
    display_engine_draw_string(rx + 4, 54, "[ <- BACK  ]", 1);

    // Курсор (однаковий для debug і release)
    if (state == UI_STATE_CH_DETAIL)
    {
        int cursor_y[] = {18, 30, 42, 54};
        if (selected_param < CH_DETAIL_ITEMS + 1)
        {
            display_engine_draw_string(rx + 0, cursor_y[selected_param], ">", 1);
        }
    }
}

// --- Рендеринг правого екрана: PRO налаштування ---
static void render_settings(int rx, uint8_t ch, int selected_param, ui_state_t state)
{
    char title[20];
    snprintf(title, sizeof(title), "-- CH%d SETUP --", ch);
    display_engine_draw_string(rx + 4, 4, title, 1);

    // #ifndef NDEBUG
    //     display_engine_draw_string(rx + 4, 16, "I: 1000 mA", 1);
    //     display_engine_draw_string(rx + 4, 28, "V: 3000 mV", 1);
    //     display_engine_draw_string(rx + 4, 40, "T:   60 C ", 1);
    //     display_engine_draw_string(rx + 4, 52, "[ <- BACK ]", 1);
    // #else
    channel_metrics_t m;
    system_state_get_metrics(ch, &m);

    char buf[20];
    snprintf(buf, sizeof(buf), "I: %4lu mA", (unsigned long)m.settings.target_current_ma);
    display_engine_draw_string(rx + 4, 16, buf, 1);

    snprintf(buf, sizeof(buf), "V: %4lu mV", (unsigned long)m.settings.cutoff_voltage_mv);
    display_engine_draw_string(rx + 4, 28, buf, 1);

    snprintf(buf, sizeof(buf), "T:  %3ld C ", (long)(m.settings.thermal_limit_mc / 1000));
    display_engine_draw_string(rx + 4, 40, buf, 1);

    display_engine_draw_string(rx + 4, 52, "[ <- BACK ]", 1);
    // #endif

    if (state == UI_STATE_SETTINGS)
    {
        int cursor_y[] = {16, 28, 40, 52};
        if (selected_param < 4)
        {
            display_engine_draw_string(rx + 0, cursor_y[selected_param], ">", 1);
        }
    }
}

// --- Головна функція рендерингу ---
static void ui_render(uint8_t selected_ch, int selected_param, ui_state_t state)
{
    display_engine_clear();

    render_left(selected_ch, state);

    int rx = 128;

    switch (state)
    {
    case UI_STATE_CH_LIST:
        // Правий екран порожній — показуємо підказку
        display_engine_draw_string(rx + 10, 28, "Select", 1);
        display_engine_draw_string(rx + 10, 40, "channel", 1);
        break;

    case UI_STATE_CH_DETAIL:
        render_ch_detail(rx, selected_ch, selected_param, state);
        break;

    case UI_STATE_SETTINGS:
        render_settings(rx, selected_ch, selected_param, state);
        break;

    case UI_STATE_GRAPH:
        display_engine_draw_string(rx + 4, 4, "-- GRAPH --", 1);
        display_engine_draw_string(rx + 10, 30, "[ COMING SOON ]", 1);
        display_engine_draw_string(rx + 20, 50, "Click to Exit", 1);
        break;
    }
}

// --- Головна таска UI ---
static void ui_task(void *pvParameters)
{
    ui_state_t current_state = UI_STATE_CH_LIST;
    int selected_ch = 0;
    int selected_param = 0;

    // Кількість пунктів у поточному меню (для wraparound)
    int menu_items = 4; // кількість каналів у CH_LIST

    while (1)
    {
        int diff = 0;
        bool btn_clicked = false;
        input_service_read(&diff, &btn_clicked);

        // --- Оновлення вибору через енкодер ---
        if (diff != 0)
        {
            switch (current_state)
            {
            case UI_STATE_CH_LIST:
                menu_items = 4;
                selected_ch = (selected_ch + diff + menu_items) % menu_items;
                break;
            case UI_STATE_CH_DETAIL:
                menu_items = 4; // Chem, START, SETTINGS, BACK
                selected_param = (selected_param + diff + menu_items) % menu_items;
                break;
            case UI_STATE_SETTINGS:
                menu_items = 4; // I, V, T, BACK
                selected_param = (selected_param + diff + menu_items) % menu_items;
                break;
            default:
                break;
            }
        }

        // --- Обробка кліку ---
        if (btn_clicked)
        {
            switch (current_state)
            {
            case UI_STATE_CH_LIST:
                // Входимо в головний екран обраного каналу
                current_state = UI_STATE_CH_DETAIL;
                selected_param = 0;
                break;

            case UI_STATE_CH_DETAIL:
                if (selected_param == 0)
                {
                    // Chem — перемикаємо хімію через system_state
                    channel_metrics_t m;
                    system_state_get_metrics(selected_ch, &m);
                    m.settings.chem = (battery_chem_t)((m.settings.chem + 1) % 3);
                    system_state_set_metrics(selected_ch, &m);
                }
                else if (selected_param == 1)
                {
                    // START — запускаємо тест
                    system_state_set_channel_state(selected_ch, STATE_PRE_CHECK);
                    current_state = UI_STATE_GRAPH; // Переходимо на екран моніторингу
                    selected_param = 0;
                }
                else if (selected_param == 2)
                {
                    // SETTINGS
                    current_state = UI_STATE_SETTINGS;
                    selected_param = 0;
                }
                else if (selected_param == 3)
                {
                    // BACK
                    current_state = UI_STATE_CH_LIST;
                    selected_param = 0;
                }
                break;

            case UI_STATE_SETTINGS:
                if (selected_param == 3)
                {
                    // BACK
                    current_state = UI_STATE_CH_DETAIL;
                    selected_param = 2; // Повертаємось на пункт SETTINGS
                }
                // TODO: редагування I, V, T через енкодер (edit mode)
                break;

            case UI_STATE_GRAPH:
                // Зупиняємо тест і повертаємось
                system_state_set_channel_state(selected_ch, STATE_IDLE);
                current_state = UI_STATE_CH_DETAIL;
                selected_param = 0;
                break;
            }
        }

        // --- Рендеринг ---
        ui_render(selected_ch, selected_param, current_state);
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
