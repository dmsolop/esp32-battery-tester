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
    UI_STATE_CH_LIST,
    UI_STATE_CH_DETAIL,
    UI_STATE_SETTINGS,
    UI_STATE_GRAPH
} ui_state_t;

#define CH_DETAIL_ITEMS 3
static const char *s_chem_names[] = {"Li-Ion", "NiMH  ", "LiFePO4"};

// --- Словник подій (всі рядки вирівняні до 14 символів для центрування) ---
static const char *get_error_msg(uint32_t error_flags)
{
    if (error_flags & ERR_OPEN_CIRCUIT)
        return " OPEN CIRCUIT ";
    if (error_flags & ERR_VOLTAGE_SAG)
        return " VOLTAGE SAG  ";
    if (error_flags & ERR_THERMAL_RUNAWAY)
        return "CRITICAL HEAT!";
    if (error_flags & ERR_OVER_TEMP)
        return " OVERHEATED   ";
    if (error_flags & ERR_OVER_CURRENT)
        return " OVER CURRENT ";
    if (error_flags & ERR_OVER_VOLTAGE)
        return " OVER VOLTAGE ";
    if (error_flags & ERR_TIMEOUT)
        return "  TIME LIMIT  ";
    if (error_flags & ERR_CAPACITY_LIMIT)
        return " CAPACITY CAP ";
    return "UNKNOWN ERROR ";
}

static const char *state_to_str(channel_state_t state)
{
    switch (state)
    {
    case STATE_IDLE:
        return "IDLE   ";
    case STATE_SELF_TEST:
        return "TEST   ";
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

// --- Рендеринг лівого екрана ---
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
                     i, (unsigned long)(m.voltage_uv / 1000), state_to_str(m.state));
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

    if (state == UI_STATE_CH_DETAIL && selected_param < CH_DETAIL_ITEMS + 1)
    {
        int cursor_y[] = {18, 30, 42, 54};
        display_engine_draw_string(rx + 0, cursor_y[selected_param], ">", 1);
    }
}

// --- Рендеринг правого екрана: PRO налаштування ---
static void render_settings(int rx, uint8_t ch, int selected_param, ui_state_t state)
{
    char title[20];
    snprintf(title, sizeof(title), "-- CH%d SETUP --", ch);
    display_engine_draw_string(rx + 4, 4, title, 1);

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

    if (state == UI_STATE_SETTINGS && selected_param < 4)
    {
        int cursor_y[] = {16, 28, 40, 52};
        display_engine_draw_string(rx + 0, cursor_y[selected_param], ">", 1);
    }
}

// --- Головна функція рендерингу ---
static void ui_render(uint8_t selected_ch, int selected_param, ui_state_t state)
{
    display_engine_clear();
    render_left(selected_ch, state);

    int rx = 128; // Координатний зсув для правого дисплея

    channel_metrics_t m;
    system_state_get_metrics(selected_ch, &m);

    // ПЕРЕХОПЛЕННЯ РЕНДЕРИНГУ: Якщо канал в аварії, блокуємо стандартний UI на правому екрані
    if (m.state == STATE_ERROR)
    {
        display_engine_draw_string(rx + 26, 12, "!!! ERROR !!!", 1);

        // Генерація блимання (400 мс)
        bool blink = (xTaskGetTickCount() / pdMS_TO_TICKS(400)) % 2 == 0;

        // Малюємо по центру (14 символів * 6px = 84px. Зсув: (128-84)/2 = 22)
        display_engine_draw_string(rx + 22, 30, get_error_msg(m.error_flags), blink ? 0 : 1);

        display_engine_draw_string(rx + 14, 50, "[ CLICK TO RESET ]", 1);
        return; // Виходимо, щоб не малювати базовий інтерфейс поверх банера
    }

    switch (state)
    {
    case UI_STATE_CH_LIST:
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
    int menu_items = 4;

    while (1)
    {
        int diff = 0;
        bool btn_clicked = false;
        input_service_read(&diff, &btn_clicked);

        channel_metrics_t cur_m;
        system_state_get_metrics(selected_ch, &cur_m);

        // --- Механізм квітування (Acknowledge) помилки ---
        if (btn_clicked && cur_m.state == STATE_ERROR)
        {
            cur_m.state = STATE_IDLE;
            cur_m.error_flags = 0;
            system_state_set_metrics(selected_ch, &cur_m);

            // Скидаємо UI на базовий вигляд каналу
            current_state = UI_STATE_CH_DETAIL;
            selected_param = 0;
            btn_clicked = false; // "Поглинаємо" клік, щоб він не викликав інших дій
        }

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
                menu_items = 4;
                selected_param = (selected_param + diff + menu_items) % menu_items;
                break;
            case UI_STATE_SETTINGS:
                menu_items = 4;
                selected_param = (selected_param + diff + menu_items) % menu_items;
                break;
            default:
                break;
            }
        }

        // --- Обробка стандартного кліку ---
        if (btn_clicked)
        {
            switch (current_state)
            {
            case UI_STATE_CH_LIST:
                current_state = UI_STATE_CH_DETAIL;
                selected_param = 0;
                break;

            case UI_STATE_CH_DETAIL:
                if (selected_param == 0)
                {
                    cur_m.settings.chem = (battery_chem_t)((cur_m.settings.chem + 1) % 3);
                    system_state_set_metrics(selected_ch, &cur_m);
                }
                else if (selected_param == 1)
                {
                    system_state_set_channel_state(selected_ch, STATE_SELF_TEST); // Замінено PRE_CHECK на SELF_TEST
                    current_state = UI_STATE_GRAPH;
                    selected_param = 0;
                }
                else if (selected_param == 2)
                {
                    current_state = UI_STATE_SETTINGS;
                    selected_param = 0;
                }
                else if (selected_param == 3)
                {
                    current_state = UI_STATE_CH_LIST;
                    selected_param = 0;
                }
                break;

            case UI_STATE_SETTINGS:
                if (selected_param == 3)
                {
                    current_state = UI_STATE_CH_DETAIL;
                    selected_param = 2;
                }
                break;

            case UI_STATE_GRAPH:
                system_state_set_channel_state(selected_ch, STATE_IDLE);
                current_state = UI_STATE_CH_DETAIL;
                selected_param = 0;
                break;
            }
        }

        ui_render(selected_ch, selected_param, current_state);
        display_engine_update();

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

esp_err_t ui_interface_init(void)
{
    ESP_ERROR_CHECK(display_engine_init());
    ESP_ERROR_CHECK(input_service_init());

    xTaskCreate(ui_task, "ui_task", 4096, NULL, configMAX_PRIORITIES - 3, NULL);
    return ESP_OK;
}