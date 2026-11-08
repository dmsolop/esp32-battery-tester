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

#define VISIBLE_ROWS 4
#define CH_DETAIL_TOTAL_ITEMS 5
#define SETTINGS_TOTAL_ITEMS 4

static const char *s_chem_names[] = {"Li-Ion", "NiMH  ", "LiFePO4"};

// --- Універсальний механізм віконної прокрутки (Viewport Scrolling) ---
static void update_scroll_offset(int selected_idx, int total_items, int *scroll_offset, int visible_rows)
{
    if (total_items <= visible_rows)
    {
        *scroll_offset = 0;
        return;
    }

    if (selected_idx < *scroll_offset)
    {
        *scroll_offset = selected_idx;
    }
    else if (selected_idx >= *scroll_offset + visible_rows)
    {
        *scroll_offset = selected_idx - visible_rows + 1;
    }
}

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
static void render_ch_detail(int rx, uint8_t ch, int selected_param, int scroll_offset, ui_state_t state)
{
    char title[20];
    snprintf(title, sizeof(title), "--- CH %d ---", ch);
    display_engine_draw_string(rx + 4, 4, title, 1);

    channel_metrics_t m;
    system_state_get_metrics(ch, &m);

    bool is_active = (m.state == STATE_SELF_TEST ||
                      m.state == STATE_PRE_CHECK ||
                      m.state == STATE_DISCHARGING ||
                      m.state == STATE_CHARGING);

    char chem_str[24];
    snprintf(chem_str, sizeof(chem_str), "Chem: %s", s_chem_names[m.settings.chem]);

    const char *items[CH_DETAIL_TOTAL_ITEMS];
    items[0] = chem_str;
    items[1] = is_active ? "[ [] STOP  ]" : "[ -> START ]";
    items[2] = "[ LIVE MON ]";
    items[3] = "[ PRO SETS ]";
    items[4] = "[ <- BACK  ]";

    int base_y[] = {16, 28, 40, 52};

    for (int row = 0; row < VISIBLE_ROWS; row++)
    {
        int item_idx = scroll_offset + row;
        if (item_idx >= CH_DETAIL_TOTAL_ITEMS)
            break;

        int y_pos = base_y[row];

        if (state == UI_STATE_CH_DETAIL && item_idx == selected_param)
        {
            display_engine_draw_string(rx + 0, y_pos, ">", 1);
        }
        display_engine_draw_string(rx + 8, y_pos, items[item_idx], 1);
    }

    // Візуальні індикатори прокрутки у правій колонці (X = rx + 120 = 248)
    if (scroll_offset > 0)
    {
        display_engine_draw_string(rx + 120, 16, "^", 1);
    }
    if (scroll_offset + VISIBLE_ROWS < CH_DETAIL_TOTAL_ITEMS)
    {
        display_engine_draw_string(rx + 120, 52, "v", 1);
    }
}

// --- Рендеринг правого екрана: PRO налаштування ---
static void render_settings(int rx, uint8_t ch, int selected_param, int scroll_offset, ui_state_t state, bool is_editing)
{
    char title[20];
    snprintf(title, sizeof(title), "-- CH%d SETUP --", ch);
    display_engine_draw_string(rx + 4, 4, title, 1);

    channel_metrics_t m;
    system_state_get_metrics(ch, &m);

    char buf_i[24];
    char buf_v[24];
    char buf_t[24];
    snprintf(buf_i, sizeof(buf_i), "I: %4lu mA", (unsigned long)m.settings.target_current_ma);
    snprintf(buf_v, sizeof(buf_v), "V: %4lu mV", (unsigned long)m.settings.cutoff_voltage_mv);
    snprintf(buf_t, sizeof(buf_t), "T:  %3ld C ", (long)(m.settings.thermal_limit_mc / 1000));

    const char *items[SETTINGS_TOTAL_ITEMS];
    items[0] = buf_i;
    items[1] = buf_v;
    items[2] = buf_t;
    items[3] = "[ <- BACK ]";

    int base_y[] = {16, 28, 40, 52};

    for (int row = 0; row < VISIBLE_ROWS; row++)
    {
        int item_idx = scroll_offset + row;
        if (item_idx >= SETTINGS_TOTAL_ITEMS)
            break;

        int y_pos = base_y[row];

        if (state == UI_STATE_SETTINGS && item_idx == selected_param)
        {
            if (is_editing && selected_param < 3)
            {
                display_engine_draw_string(rx + 0, y_pos, ">", 1);
                display_engine_draw_string(rx + 82, y_pos, "<", 1);
            }
            else
            {
                display_engine_draw_string(rx + 0, y_pos, ">", 1);
            }
        }
        display_engine_draw_string(rx + 8, y_pos, items[item_idx], 1);
    }

    // Візуальні індикатори прокрутки у правій колонці (X = rx + 120 = 248)
    if (scroll_offset > 0)
    {
        display_engine_draw_string(rx + 120, 16, "^", 1);
    }
    if (scroll_offset + VISIBLE_ROWS < SETTINGS_TOTAL_ITEMS)
    {
        display_engine_draw_string(rx + 120, 52, "v", 1);
    }
}

// --- Головна функція рендерингу ---
static void ui_render(uint8_t selected_ch, int selected_param, int scroll_offset, ui_state_t state, bool is_editing)
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
        render_ch_detail(rx, selected_ch, selected_param, scroll_offset, state);
        break;

    case UI_STATE_SETTINGS:
        render_settings(rx, selected_ch, selected_param, scroll_offset, state, is_editing);
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
    int scroll_offset = 0;
    int menu_items = 4;
    bool is_editing = false;

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
            scroll_offset = 0;
            is_editing = false;
            btn_clicked = false; // "Поглинаємо" клік, щоб він не викликав інших дій
        }

        // --- Оновлення вибору або редагування значення через енкодер ---
        if (diff != 0)
        {
            if (current_state == UI_STATE_SETTINGS && is_editing)
            {
                // Режим редагування параметрів у Settings
                if (selected_param == 0)
                {
                    // Струм: крок 50 мА, діапазон 50..3000 мА
                    int32_t val = (int32_t)cur_m.settings.target_current_ma + diff * 50;
                    if (val < 50)
                        val = 50;
                    if (val > 3000)
                        val = 3000;
                    cur_m.settings.target_current_ma = (uint32_t)val;
                    system_state_set_metrics(selected_ch, &cur_m);
                }
                else if (selected_param == 1)
                {
                    // Напруга відсічки: крок 50 мВ, діапазон 2500..4500 мВ
                    int32_t val = (int32_t)cur_m.settings.cutoff_voltage_mv + diff * 50;
                    if (val < 2500)
                        val = 2500;
                    if (val > 4500)
                        val = 4500;
                    cur_m.settings.cutoff_voltage_mv = (uint32_t)val;
                    system_state_set_metrics(selected_ch, &cur_m);
                }
                else if (selected_param == 2)
                {
                    // Температура: крок 1 °C (1000 mc), діапазон 30..80 °C (30000..80000 mc)
                    int32_t cur_temp_c = cur_m.settings.thermal_limit_mc / 1000;
                    int32_t val = cur_temp_c + diff * 1;
                    if (val < 30)
                        val = 30;
                    if (val > 80)
                        val = 80;
                    cur_m.settings.thermal_limit_mc = val * 1000;
                    system_state_set_metrics(selected_ch, &cur_m);
                    system_state_update_sensor_limits(selected_ch);
                }
            }
            else
            {
                switch (current_state)
                {
                case UI_STATE_CH_LIST:
                    menu_items = CONFIG_MAX_CHANNELS;
                    selected_ch = (selected_ch + diff + menu_items) % menu_items;
                    break;
                case UI_STATE_CH_DETAIL:
                    menu_items = CH_DETAIL_TOTAL_ITEMS;
                    selected_param = (selected_param + diff + menu_items) % menu_items;
                    update_scroll_offset(selected_param, menu_items, &scroll_offset, VISIBLE_ROWS);
                    break;
                case UI_STATE_SETTINGS:
                    menu_items = SETTINGS_TOTAL_ITEMS;
                    selected_param = (selected_param + diff + menu_items) % menu_items;
                    update_scroll_offset(selected_param, menu_items, &scroll_offset, VISIBLE_ROWS);
                    break;
                default:
                    break;
                }
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
                scroll_offset = 0;
                break;

            case UI_STATE_CH_DETAIL:
                if (selected_param == 0)
                {
                    // Дозволено перемикання хімії лише коли канал у STATE_IDLE або STATE_FINISHED
                    if (cur_m.state == STATE_IDLE || cur_m.state == STATE_FINISHED)
                    {
                        battery_chem_t next_chem = (battery_chem_t)((cur_m.settings.chem + 1) % 3);
                        system_state_set_chemistry(selected_ch, next_chem);
                    }
                }
                else if (selected_param == 1)
                {
                    // Динамічна дія запуску/зупинки
                    bool is_active = (cur_m.state == STATE_SELF_TEST ||
                                      cur_m.state == STATE_PRE_CHECK ||
                                      cur_m.state == STATE_DISCHARGING ||
                                      cur_m.state == STATE_CHARGING);
                    if (is_active)
                    {
                        // Негайна зупинка розряду
                        system_state_set_channel_state(selected_ch, STATE_IDLE);
                    }
                    else
                    {
                        // Запуск розряду
                        system_state_set_channel_state(selected_ch, STATE_SELF_TEST);
                    }
                }
                else if (selected_param == 2)
                {
                    // LIVE MONITOR
                    current_state = UI_STATE_GRAPH;
                    selected_param = 0;
                    scroll_offset = 0;
                }
                else if (selected_param == 3)
                {
                    // PRO SETTINGS
                    current_state = UI_STATE_SETTINGS;
                    selected_param = 0;
                    scroll_offset = 0;
                    is_editing = false;
                }
                else if (selected_param == 4)
                {
                    // BACK TO CH LIST
                    current_state = UI_STATE_CH_LIST;
                    selected_param = 0;
                    scroll_offset = 0;
                }
                break;

            case UI_STATE_SETTINGS:
                if (selected_param == 3)
                {
                    // BACK TO CH DETAIL
                    current_state = UI_STATE_CH_DETAIL;
                    selected_param = 3; // Повертаємо курсор на пункт [ PRO SETS ]
                    update_scroll_offset(selected_param, CH_DETAIL_TOTAL_ITEMS, &scroll_offset, VISIBLE_ROWS);
                    is_editing = false;
                }
                else if (selected_param >= 0 && selected_param <= 2)
                {
                    // Перемикання режиму редагування на параметрах 0, 1, 2
                    is_editing = !is_editing;
                }
                break;

            case UI_STATE_GRAPH:
                // Вихід з екрана моніторингу назад у CH_DETAIL БЕЗ зупинки розряду
                current_state = UI_STATE_CH_DETAIL;
                selected_param = 2; // Курсор на [ LIVE MON ]
                update_scroll_offset(selected_param, CH_DETAIL_TOTAL_ITEMS, &scroll_offset, VISIBLE_ROWS);
                break;
            }
        }

        ui_render(selected_ch, selected_param, scroll_offset, current_state, is_editing);
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