#include "ui_interface.h"
#include "display_engine.h"
#include "input_service.h"
#include "system_state.h"
#include "load_control.h"
#include "adc_driver.h"
#include "temp_service.h"
#include "buzzer_service.h"
#include "icons.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "UI_INTERFACE";

// --- Стани кінцевого автомата UI ---
typedef enum
{
    UI_STATE_CH_LIST,
    UI_STATE_CH_DETAIL,
    UI_STATE_SETTINGS,
    UI_STATE_GRAPH,
    UI_STATE_TEST_DIAG,
    UI_STATE_SENSOR_BIND
} ui_state_t;

#define VISIBLE_ROWS 4
#define CH_DETAIL_TOTAL_ITEMS 5
#define SETTINGS_TOTAL_ITEMS 8
#define LIVE_MONITOR_TOTAL_ITEMS 8
#define TEST_DIAG_TOTAL_ITEMS 10
#define SENSOR_BIND_TOTAL_ITEMS (MAX_SENSORS_PER_CHANNEL + 2)

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

static const char *state_to_short_str(channel_state_t state)
{
    switch (state)
    {
    case STATE_IDLE:
        return "IDLE";
    case STATE_SELF_TEST:
        return "TEST";
    case STATE_PRE_CHECK:
        return "PRE";
    case STATE_CHARGING:
        return "CHRG";
    case STATE_DISCHARGING:
        return "DISCH";
    case STATE_DCIR_TEST:
        return "DCIR";
    case STATE_FINISHED:
        return "DONE";
    case STATE_ERROR:
        return "ERR";
    default:
        return "???";
    }
}

static const char *soh_to_str(soh_verdict_t soh)
{
    switch (soh)
    {
    case SOH_EXCELLENT:
    case SOH_GOOD:
        return "GOOD";
    case SOH_DEGRADED:
        return "DEGR";
    case SOH_DEAD:
        return "DEAD";
    case SOH_UNKNOWN:
    default:
        return "---";
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
                      m.state == STATE_DCIR_TEST ||
                      m.state == STATE_CHARGING);

    char chem_str[24];
    snprintf(chem_str, sizeof(chem_str), "Chemistry: %s", s_chem_names[m.settings.chem]);

    const char *items[CH_DETAIL_TOTAL_ITEMS];
    items[0] = is_active ? "[ [] STOP      ]" : "[ -> START     ]";
    items[1] = "[ -> DCIR TEST ]";
    items[2] = chem_str;
    items[3] = "[ TESTS & SAFE ]";
    items[4] = "[ <- BACK      ]";

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
    char buf_cap[24];
    char buf_tim[24];
    char buf_buzzer[24];
    snprintf(buf_i, sizeof(buf_i), "I: %4lu mA", (unsigned long)m.settings.target_current_ma);
    snprintf(buf_v, sizeof(buf_v), "V: %4lu mV", (unsigned long)m.settings.cutoff_voltage_mv);
    snprintf(buf_t, sizeof(buf_t), "T:  %3ld C ", (long)(m.settings.thermal_limit_mc / 1000));
    snprintf(buf_cap, sizeof(buf_cap), "CapLim: %4lu mAh", (unsigned long)m.settings.capacity_limit_mah);
    snprintf(buf_tim, sizeof(buf_tim), "TimeLim: %3lu min", (unsigned long)(m.settings.time_limit_s / 60));
    snprintf(buf_buzzer, sizeof(buf_buzzer), "Buzzer:    %3u%%", (unsigned int)buzzer_get_volume());

    const char *items[SETTINGS_TOTAL_ITEMS];
    items[0] = buf_i;
    items[1] = buf_v;
    items[2] = buf_t;
    items[3] = buf_cap;
    items[4] = buf_tim;
    items[5] = buf_buzzer;
    items[6] = "[ Reset Defaults ]";
    items[7] = "[ <- BACK ]";

    int base_y[] = {16, 28, 40, 52};

    for (int row = 0; row < VISIBLE_ROWS; row++)
    {
        int item_idx = scroll_offset + row;
        if (item_idx >= SETTINGS_TOTAL_ITEMS)
            break;

        int y_pos = base_y[row];

        if (state == UI_STATE_SETTINGS && item_idx == selected_param)
        {
            if (is_editing && selected_param <= 5)
            {
                display_engine_draw_string(rx + 0, y_pos, ">", 1);
                display_engine_draw_string(rx + 112, y_pos, "<", 1);
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

// --- Рендеринг правого екрана: Живий моніторинг параметрів каналу ---
static void render_live_monitor(int rx, uint8_t ch, int selected_param, int scroll_offset, ui_state_t state)
{
    channel_metrics_t m;
    system_state_get_metrics(ch, &m);

    // Заголовок правого екрана: CH<X> LIVE [<STATE>]
    char title[64];
    snprintf(title, sizeof(title), "CH%d LIVE [%s]", ch, state_to_short_str(m.state));
    display_engine_draw_string(rx + 4, 4, title, 1);

    // Рядок 0: V:%4dmV I:%4dmA
    char row0[64];
    snprintf(row0, sizeof(row0), "V:%4lumV I:%4lumA",
             (unsigned long)(m.voltage_uv / 1000),
             (unsigned long)(m.current_ua / 1000));

    // Рядок 1: DCIR:%4lumO P:%lu.%luW
    // Розрахунок потужності P_mw = (uint64_t)abs(m.voltage_uv) * abs(m.current_ua) / 1000000000ULL
    uint64_t p_mw = ((uint64_t)m.voltage_uv * (uint64_t)m.current_ua) / 1000000000ULL;
    unsigned int p_w_int = (unsigned int)((p_mw / 1000ULL) % 1000U);
    unsigned int p_w_tenth = (unsigned int)((p_mw % 1000ULL) / 100ULL);
    char row1[128];
    int len1 = snprintf(row1, sizeof(row1), "DCIR:%4umO P:%u.%u",
                        (unsigned int)(m.internal_res_mohm % 10000U),
                        p_w_int,
                        p_w_tenth);
    if (len1 >= 0 && len1 < (int)sizeof(row1) - 1)
    {
        row1[len1] = 'W';
        row1[len1 + 1] = '\0';
    }

    // Рядок 2: Температури 2 радіаторів «Sandwich» (HS1, HS2)
    const temp_sensor_data_t *hs1 = NULL;
    const temp_sensor_data_t *hs2 = NULL;
    const temp_sensor_data_t *cell = NULL;

    for (int i = 0; i < MAX_SENSORS_PER_CHANNEL; i++)
    {
        if (m.temp_sensors[i].is_bound)
        {
            if (m.temp_sensors[i].role == SENSOR_ROLE_HEATSINK)
            {
                if (!hs1)
                {
                    hs1 = &m.temp_sensors[i];
                }
                else if (!hs2)
                {
                    hs2 = &m.temp_sensors[i];
                }
            }
            else if (m.temp_sensors[i].role == SENSOR_ROLE_CELL)
            {
                if (!cell)
                {
                    cell = &m.temp_sensors[i];
                }
            }
        }
    }

    char hs1_str[16];
    if (hs1)
    {
        int32_t t = hs1->current_temp_mc;
        int32_t deg = t / 1000;
        int32_t dec = abs(t % 1000) / 100;
        snprintf(hs1_str, sizeof(hs1_str), "%2ld.%ldC", (long)deg, (long)dec);
    }
    else
    {
        snprintf(hs1_str, sizeof(hs1_str), "--.-C");
    }

    char hs2_str[16];
    if (hs2)
    {
        int32_t t = hs2->current_temp_mc;
        int32_t deg = t / 1000;
        int32_t dec = abs(t % 1000) / 100;
        snprintf(hs2_str, sizeof(hs2_str), "%2ld.%ldC", (long)deg, (long)dec);
    }
    else
    {
        snprintf(hs2_str, sizeof(hs2_str), "--.-C");
    }

    char row2[64];
    snprintf(row2, sizeof(row2), "HS1:%s HS2:%s", hs1_str, hs2_str);

    // Рядок 3: Температура комірки та ліміт: Cell:%2d.%dC (M:%2dC)
    long therm_limit_c = (long)(m.settings.thermal_limit_mc / 1000);
    char row3[64];
    if (cell)
    {
        int32_t t = cell->current_temp_mc;
        int32_t deg = t / 1000;
        int32_t dec = abs(t % 1000) / 100;
        snprintf(row3, sizeof(row3), "Cell:%2ld.%ldC (M:%2ldC)", (long)deg, (long)dec, therm_limit_c);
    }
    else
    {
        snprintf(row3, sizeof(row3), "Cell:--.-C (M:%2ldC)", therm_limit_c);
    }

    // Рядок 4: C:%4lumAh E:%4lumWh
    char row4[64];
    snprintf(row4, sizeof(row4), "C:%4lumAh E:%4lumWh",
             (unsigned long)m.capacity_mah,
             (unsigned long)m.energy_mwh);

    // Рядок 5: OCV:%4dmV Pk:%4d
    char row5[64];
    snprintf(row5, sizeof(row5), "OCV:%4lumV Pk:%4lu",
             (unsigned long)(m.ocv_uv / 1000),
             (unsigned long)(m.peak_voltage_uv / 1000));

    // Рядок 6: T:%02lu:%02lu SOH:%s
    uint64_t elapsed_s = m.elapsed_time_us / 1000000ULL;
    unsigned long minutes = (unsigned long)(elapsed_s / 60ULL);
    unsigned long seconds = (unsigned long)(elapsed_s % 60ULL);
    char row6[64];
    snprintf(row6, sizeof(row6), "T:%02lu:%02lu SOH:%s",
             minutes,
             seconds,
             soh_to_str(m.soh));

    bool is_active = (m.state == STATE_SELF_TEST ||
                      m.state == STATE_PRE_CHECK ||
                      m.state == STATE_DISCHARGING ||
                      m.state == STATE_DCIR_TEST ||
                      m.state == STATE_CHARGING);

    const char *row7 = is_active ? "[ [] STOP LOAD  ]" : "[ <- BACK       ]";

    const char *items[LIVE_MONITOR_TOTAL_ITEMS];
    items[0] = row0;
    items[1] = row1;
    items[2] = row2;
    items[3] = row3;
    items[4] = row4;
    items[5] = row5;
    items[6] = row6;
    items[7] = row7;

    int base_y[] = {16, 28, 40, 52};

    for (int row = 0; row < VISIBLE_ROWS; row++)
    {
        int item_idx = scroll_offset + row;
        if (item_idx >= LIVE_MONITOR_TOTAL_ITEMS)
            break;

        int y_pos = base_y[row];

        if (state == UI_STATE_GRAPH && item_idx == selected_param)
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
    if (scroll_offset + VISIBLE_ROWS < LIVE_MONITOR_TOTAL_ITEMS)
    {
        display_engine_draw_string(rx + 120, 52, "v", 1);
    }
}

// --- Рендеринг правого екрана: Тести, діагностика та безпека ---
static void render_test_diag(int rx, uint8_t ch, int selected_param, int scroll_offset, ui_state_t state, bool is_editing, uint16_t raw_pwm_val)
{
    char title[20];
    snprintf(title, sizeof(title), "- CH%d TEST/DIAG -", ch);
    display_engine_draw_string(rx + 4, 4, title, 1);

    channel_metrics_t m;
    system_state_get_metrics(ch, &m);

    // Рахуємо кількість прив'язаних 1-Wire датчиків
    int bound_sensors = 0;
    int hs_count = 0;
    int cell_count = 0;
    for (int i = 0; i < MAX_SENSORS_PER_CHANNEL; i++)
    {
        if (m.temp_sensors[i].is_bound)
        {
            bound_sensors++;
            if (m.temp_sensors[i].role == SENSOR_ROLE_HEATSINK)
                hs_count++;
            else if (m.temp_sensors[i].role == SENSOR_ROLE_CELL)
                cell_count++;
        }
    }

    uint8_t fan_pct = load_control_get_fan_pwm(ch);

    // Візуальний фідбек для тарування нуля струму: показуємо зафіксований офсет
    int32_t zero_off_ua = adc_driver_get_zero_offset_current(ch);
    char buf_zerocal[24];
    if (zero_off_ua == 0)
    {
        snprintf(buf_zerocal, sizeof(buf_zerocal), "[ Zero Calib REL ]");
    }
    else
    {
        int32_t off_ma_int = zero_off_ua / 1000;
        int32_t off_ma_frac = abs(zero_off_ua % 1000) / 100;
        snprintf(buf_zerocal, sizeof(buf_zerocal), "ZCal:%+ld.%ldmA[OK]", (long)off_ma_int, (long)off_ma_frac);
    }

    char buf_pwm[24];
    char buf_fan[24];
    char buf_sag[24];
    char buf_faults[24];
    char buf_1w[24];
    snprintf(buf_pwm, sizeof(buf_pwm), "RawPWM: %4u", raw_pwm_val);
    snprintf(buf_fan, sizeof(buf_fan), "FanPWM: %3u%%", (unsigned int)fan_pct);
    snprintf(buf_sag, sizeof(buf_sag), "DCIR:%4umO [PUL]", (unsigned int)(m.internal_res_mohm % 10000U));
    snprintf(buf_faults, sizeof(buf_faults), "Fault:0x%02lX [CLR]", (unsigned long)m.error_flags);
    snprintf(buf_1w, sizeof(buf_1w), "1W:H%d/C%d [ROLES]", hs_count, cell_count);

    const char *items[TEST_DIAG_TOTAL_ITEMS];
    items[0] = "[ HW Self-Test ]";
    items[1] = "[ Pre-Check OCV ]";
    items[2] = buf_zerocal;
    items[3] = buf_pwm;
    items[4] = buf_fan;
    items[5] = buf_sag;
    items[6] = "[ PRO SETS ]";
    items[7] = buf_faults;
    items[8] = buf_1w;
    items[9] = "[ <- BACK ]";

    int base_y[] = {16, 28, 40, 52};

    for (int row = 0; row < VISIBLE_ROWS; row++)
    {
        int item_idx = scroll_offset + row;
        if (item_idx >= TEST_DIAG_TOTAL_ITEMS)
            break;

        int y_pos = base_y[row];

        if (state == UI_STATE_TEST_DIAG && item_idx == selected_param)
        {
            if (is_editing && (selected_param == 3 || selected_param == 4))
            {
                display_engine_draw_string(rx + 0, y_pos, ">", 1);
                display_engine_draw_string(rx + 98, y_pos, "<", 1);
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
    if (scroll_offset + VISIBLE_ROWS < TEST_DIAG_TOTAL_ITEMS)
    {
        display_engine_draw_string(rx + 120, 52, "v", 1);
    }
}

// --- Рендеринг екрана прив'язки 1-Wire датчиків до ролей ---
static void render_sensor_bind(int rx, uint8_t ch, int selected_param, int scroll_offset, ui_state_t state)
{
    char title[24];
    snprintf(title, sizeof(title), "- CH%d 1W SENSORS -", ch);
    display_engine_draw_string(rx + 4, 4, title, 1);

    channel_metrics_t m;
    system_state_get_metrics(ch, &m);

    char items[SENSOR_BIND_TOTAL_ITEMS][24];
    for (int i = 0; i < MAX_SENSORS_PER_CHANNEL; i++)
    {
        if (m.temp_sensors[i].is_bound)
        {
            const char *role_tag = "NONE";
            if (m.temp_sensors[i].role == SENSOR_ROLE_HEATSINK)
            {
                role_tag = "HS  ";
            }
            else if (m.temp_sensors[i].role == SENSOR_ROLE_CELL)
            {
                role_tag = "CELL";
            }

            int32_t t = m.temp_sensors[i].current_temp_mc;
            int32_t deg = t / 1000;
            // Показуємо індекс, останні 2 байти ROM (CRC / унікальний ID) та поточну температуру в реальному часі
            snprintf(items[i], sizeof(items[i]), "#%d:%02X%02X %2ldC %s",
                     i, m.temp_sensors[i].rom[6], m.temp_sensors[i].rom[7], (long)deg, role_tag);
        }
        else
        {
            snprintf(items[i], sizeof(items[i]), "#%d: --.-C [EMPTY]", i);
        }
    }

    snprintf(items[MAX_SENSORS_PER_CHANNEL], sizeof(items[MAX_SENSORS_PER_CHANNEL]), "[ Rescan 1-Wire ]");
    snprintf(items[MAX_SENSORS_PER_CHANNEL + 1], sizeof(items[MAX_SENSORS_PER_CHANNEL + 1]), "[ <- BACK ]");

    int base_y[] = {16, 28, 40, 52};

    for (int row = 0; row < VISIBLE_ROWS; row++)
    {
        int item_idx = scroll_offset + row;
        if (item_idx >= SENSOR_BIND_TOTAL_ITEMS)
            break;

        int y_pos = base_y[row];

        if (state == UI_STATE_SENSOR_BIND && item_idx == selected_param)
        {
            display_engine_draw_string(rx + 0, y_pos, ">", 1);
        }
        display_engine_draw_string(rx + 8, y_pos, items[item_idx], 1);
    }

    // Візуальні індикатори прокрутки (X = rx + 120 = 248)
    if (scroll_offset > 0)
    {
        display_engine_draw_string(rx + 120, 16, "^", 1);
    }
    if (scroll_offset + VISIBLE_ROWS < SENSOR_BIND_TOTAL_ITEMS)
    {
        display_engine_draw_string(rx + 120, 52, "v", 1);
    }
}

// --- Головна функція рендерингу ---
static void ui_render(uint8_t selected_ch, int selected_param, int scroll_offset, ui_state_t state, bool is_editing, uint16_t raw_pwm_val)
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
        render_live_monitor(rx, selected_ch, selected_param, scroll_offset, state);
        break;

    case UI_STATE_TEST_DIAG:
        render_test_diag(rx, selected_ch, selected_param, scroll_offset, state, is_editing, raw_pwm_val);
        break;

    case UI_STATE_SENSOR_BIND:
        render_sensor_bind(rx, selected_ch, selected_param, scroll_offset, state);
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
    uint16_t raw_pwm_val = 0;

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
                else if (selected_param == 3)
                {
                    // Ліміт ємності: крок 250 мА·год, діапазон 500..20000 мА·год
                    int32_t val = (int32_t)cur_m.settings.capacity_limit_mah + diff * 250;
                    if (val < 500)
                        val = 500;
                    if (val > 20000)
                        val = 20000;
                    cur_m.settings.capacity_limit_mah = (uint32_t)val;
                    system_state_set_metrics(selected_ch, &cur_m);
                }
                else if (selected_param == 4)
                {
                    // Ліміт часу: крок 15 хв (900 с), діапазон 15..1440 хв (900..86400 с)
                    int32_t cur_min = (int32_t)(cur_m.settings.time_limit_s / 60);
                    int32_t val_min = cur_min + diff * 15;
                    if (val_min < 15)
                        val_min = 15;
                    if (val_min > 1440)
                        val_min = 1440;
                    cur_m.settings.time_limit_s = (uint32_t)(val_min * 60);
                    system_state_set_metrics(selected_ch, &cur_m);
                }
                else if (selected_param == 5)
                {
                    // Гучність бузера: крок 20%, діапазон 0..100%
                    int32_t cur_vol = (int32_t)buzzer_get_volume();
                    int32_t val_vol = cur_vol + diff * 20;
                    if (val_vol < 0)
                        val_vol = 0;
                    if (val_vol > 100)
                        val_vol = 100;
                    buzzer_set_volume((uint8_t)val_vol);
                    buzzer_play(BUZZER_EVENT_CLICK);
                }
            }
            else if (current_state == UI_STATE_TEST_DIAG && is_editing)
            {
                // Режим редагування параметрів у Test & Diag
                if (selected_param == 3)
                {
                    // Тестовий ШІМ навантаження: крок 50 відліків, діапазон 0..4095
                    int32_t val = (int32_t)raw_pwm_val + diff * 50;
                    if (val < 0)
                        val = 0;
                    if (val > 4095)
                        val = 4095;
                    raw_pwm_val = (uint16_t)val;
                    load_control_set_raw_pwm(selected_ch, raw_pwm_val);
                }
                else if (selected_param == 4)
                {
                    // Керування ШІМ вентилятора: крок 10%, діапазон 0..100%
                    uint8_t cur_fan = load_control_get_fan_pwm(selected_ch);
                    int32_t val_fan = (int32_t)cur_fan + diff * 10;
                    if (val_fan < 0)
                        val_fan = 0;
                    if (val_fan > 100)
                        val_fan = 100;
                    load_control_set_fan_pwm(selected_ch, (uint8_t)val_fan);
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
                case UI_STATE_GRAPH:
                    menu_items = LIVE_MONITOR_TOTAL_ITEMS;
                    selected_param = (selected_param + diff + menu_items) % menu_items;
                    update_scroll_offset(selected_param, menu_items, &scroll_offset, VISIBLE_ROWS);
                    break;
                case UI_STATE_TEST_DIAG:
                    menu_items = TEST_DIAG_TOTAL_ITEMS;
                    selected_param = (selected_param + diff + menu_items) % menu_items;
                    update_scroll_offset(selected_param, menu_items, &scroll_offset, VISIBLE_ROWS);
                    break;
                case UI_STATE_SENSOR_BIND:
                    menu_items = SENSOR_BIND_TOTAL_ITEMS;
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
                buzzer_play(BUZZER_EVENT_CLICK);
                break;

            case UI_STATE_CH_DETAIL:
                if (selected_param == 0)
                {
                    // Динамічна дія запуску/зупинки повного тесту
                    bool is_active = (cur_m.state == STATE_SELF_TEST ||
                                      cur_m.state == STATE_PRE_CHECK ||
                                      cur_m.state == STATE_DISCHARGING ||
                                      cur_m.state == STATE_DCIR_TEST ||
                                      cur_m.state == STATE_CHARGING);
                    if (is_active)
                    {
                        // Негайна зупинка розряду з апаратним обнуленням ШІМ
                        load_control_stop(selected_ch);
                        buzzer_play(BUZZER_EVENT_WARNING);
                    }
                    else
                    {
                        // Запуск розряду
                        system_state_set_channel_state(selected_ch, STATE_SELF_TEST);
                        buzzer_play(BUZZER_EVENT_CONFIRM);
                    }
                }
                else if (selected_param == 1)
                {
                    // [ -> DCIR TEST ]: запуск 10-секундного високоточного тесту DCIR
                    load_control_start_dcir_test(selected_ch);
                    current_state = UI_STATE_GRAPH;
                    selected_param = 0;
                    scroll_offset = 0;
                }
                else if (selected_param == 2)
                {
                    // Дозволено перемикання хімії лише коли канал у STATE_IDLE або STATE_FINISHED
                    if (cur_m.state == STATE_IDLE || cur_m.state == STATE_FINISHED)
                    {
                        battery_chem_t next_chem = (battery_chem_t)((cur_m.settings.chem + 1) % 3);
                        system_state_set_chemistry(selected_ch, next_chem);
                        buzzer_play(BUZZER_EVENT_CLICK);
                    }
                }
                else if (selected_param == 3)
                {
                    // TESTS & SAFE DIAG
                    current_state = UI_STATE_TEST_DIAG;
                    selected_param = 0;
                    scroll_offset = 0;
                    is_editing = false;
                    buzzer_play(BUZZER_EVENT_CLICK);
                }
                else if (selected_param == 4)
                {
                    // BACK TO CH LIST
                    current_state = UI_STATE_CH_LIST;
                    selected_param = 0;
                    scroll_offset = 0;
                    buzzer_play(BUZZER_EVENT_CLICK);
                }
                break;

            case UI_STATE_SETTINGS:
                if (selected_param == 7)
                {
                    // BACK TO CH DETAIL
                    current_state = UI_STATE_TEST_DIAG;
                    selected_param = 0;
                    update_scroll_offset(selected_param, TEST_DIAG_TOTAL_ITEMS, &scroll_offset, VISIBLE_ROWS);
                    is_editing = false;
                    buzzer_play(BUZZER_EVENT_CLICK);
                }
                else if (selected_param == 6)
                {
                    // [ Reset Defaults ]: скидання до безпечних ROM-значень поточної хімії
                    system_state_set_chemistry(selected_ch, cur_m.settings.chem);
                    buzzer_play(BUZZER_EVENT_CONFIRM);
                }
                else if (selected_param >= 0 && selected_param <= 5)
                {
                    // Перемикання режиму редагування на параметрах 0..5 (I, V, T, CapLim, TimeLim, Buzzer)
                    is_editing = !is_editing;
                    buzzer_play(BUZZER_EVENT_CLICK);
                }
                break;

            case UI_STATE_TEST_DIAG:
                if (selected_param == 0)
                {
                    // [ HW Self-Test ]: запуск діагностичного самотестування силового тракту (без переходу у розряд)
                    load_control_start_diag_self_test(selected_ch);
                    current_state = UI_STATE_GRAPH;
                    selected_param = 0;
                    scroll_offset = 0;
                    is_editing = false;
                    buzzer_play(BUZZER_EVENT_CONFIRM);
                }
                else if (selected_param == 1)
                {
                    // [ Pre-Check OCV ]: запуск діагностичного вимірювання ЕРС (без переходу у розряд)
                    load_control_start_diag_precheck(selected_ch);
                    current_state = UI_STATE_GRAPH;
                    selected_param = 0;
                    scroll_offset = 0;
                    is_editing = false;
                    buzzer_play(BUZZER_EVENT_CONFIRM);
                }
                else if (selected_param == 2)
                {
                    // [ Zero Calib REL ]: тарування нуля через неблокуючий виклик
                    load_control_request_zero_calib(selected_ch);
                }
                else if (selected_param == 3)
                {
                    // RawPWM: перемикання режиму інтерактивного редагування значення ШІМ навантаження
                    is_editing = !is_editing;
                    buzzer_play(BUZZER_EVENT_CLICK);
                }
                else if (selected_param == 4)
                {
                    // FanPWM: перемикання режиму інтерактивного редагування ШІМ кулера
                    is_editing = !is_editing;
                    buzzer_play(BUZZER_EVENT_CLICK);
                }
                else if (selected_param == 5)
                {
                    // [ DCIR Test ]: запуск 10-секундного високоточного тесту DCIR з переходом на Live Monitor
                    load_control_start_dcir_test(selected_ch);
                    current_state = UI_STATE_GRAPH;
                    selected_param = 0;
                    scroll_offset = 0;
                    is_editing = false;
                }
                else if (selected_param == 6)
                {
                    // [ PRO SETS ]: Перехід до розширених налаштувань
                    current_state = UI_STATE_SETTINGS;
                    selected_param = 0;
                    scroll_offset = 0;
                    is_editing = false;
                    buzzer_play(BUZZER_EVENT_CLICK);
                }
                else if (selected_param == 7)
                {
                    // [ Clear Faults ]: ручне квітування / скидання аварійного стану каналу з меню тестів
                    cur_m.error_flags = 0;
                    if (cur_m.state == STATE_ERROR)
                    {
                        cur_m.state = STATE_IDLE;
                    }
                    system_state_set_metrics(selected_ch, &cur_m);
                    buzzer_play(BUZZER_EVENT_CONFIRM);
                    ESP_LOGI(TAG, "CH%d: Faults cleared manually from TEST/DIAG menu.", selected_ch);
                }
                else if (selected_param == 8)
                {
                    // [ 1W Sensors & Roles ]: Перехід до екрана детальної прив'язки датчиків
                    current_state = UI_STATE_SENSOR_BIND;
                    selected_param = 0;
                    scroll_offset = 0;
                    is_editing = false;
                    buzzer_play(BUZZER_EVENT_CLICK);
                }
                else if (selected_param == 9)
                {
                    // BACK TO CH DETAIL
                    // Якщо був виставлений ручний тестовий ШІМ, безпечно скидаємо його при виході
                    if (raw_pwm_val > 0)
                    {
                        raw_pwm_val = 0;
                        load_control_set_raw_pwm(selected_ch, 0);
                    }
                    // Повертаємо кулер у режим автоматичного терморегулювання
                    load_control_set_fan_manual_override(selected_ch, false);

                    current_state = UI_STATE_CH_DETAIL;
                    selected_param = 3; // Повертаємо курсор на [ TESTS & SAFE ]
                    update_scroll_offset(selected_param, CH_DETAIL_TOTAL_ITEMS, &scroll_offset, VISIBLE_ROWS);
                    is_editing = false;
                    buzzer_play(BUZZER_EVENT_CLICK);
                }
                break;

            case UI_STATE_SENSOR_BIND:
                if (selected_param < MAX_SENSORS_PER_CHANNEL)
                {
                    // Клік по датчику #0..#5: циклічне перемикання ролі з негайним записом у EEPROM DS18B20
                    if (cur_m.temp_sensors[selected_param].is_bound)
                    {
                        sensor_role_t next_role = SENSOR_ROLE_NONE;
                        if (cur_m.temp_sensors[selected_param].role == SENSOR_ROLE_NONE)
                        {
                            next_role = SENSOR_ROLE_HEATSINK;
                        }
                        else if (cur_m.temp_sensors[selected_param].role == SENSOR_ROLE_HEATSINK)
                        {
                            next_role = SENSOR_ROLE_CELL;
                        }
                        else
                        {
                            next_role = SENSOR_ROLE_HEATSINK;
                        }
                        temp_service_bind_sensor_role(selected_ch, selected_param, next_role);
                        buzzer_play(BUZZER_EVENT_CONFIRM);
                    }
                }
                else if (selected_param == MAX_SENSORS_PER_CHANNEL)
                {
                    // [ Rescan 1-Wire ]: повторне сканування шини для виявлення щойно підключених датчиків
                    temp_service_rescan(selected_ch);
                    buzzer_play(BUZZER_EVENT_CONFIRM);
                }
                else if (selected_param == MAX_SENSORS_PER_CHANNEL + 1)
                {
                    // [ <- BACK ]: повернення в меню TESTS & SAFE
                    current_state = UI_STATE_TEST_DIAG;
                    selected_param = 8; // повертаємо курсор на пункт 1W
                    update_scroll_offset(selected_param, TEST_DIAG_TOTAL_ITEMS, &scroll_offset, VISIBLE_ROWS);
                    buzzer_play(BUZZER_EVENT_CLICK);
                }
                break;

            case UI_STATE_GRAPH:
                // Якщо обрано рядок 7:
                // Якщо канал активний -> [ [] STOP LOAD ] (зупиняємо розряд через load_control_stop)
                // Якщо неактивний -> повертаємося назад у CH DETAIL
                if (selected_param == 7)
                {
                    bool is_active = (cur_m.state == STATE_SELF_TEST ||
                                      cur_m.state == STATE_PRE_CHECK ||
                                      cur_m.state == STATE_DISCHARGING ||
                                      cur_m.state == STATE_DCIR_TEST ||
                                      cur_m.state == STATE_CHARGING);
                    if (is_active)
                    {
                        load_control_stop(selected_ch);
                        buzzer_play(BUZZER_EVENT_WARNING);
                    }
                    else
                    {
                        current_state = UI_STATE_CH_DETAIL;
                        selected_param = 0;
                        update_scroll_offset(selected_param, CH_DETAIL_TOTAL_ITEMS, &scroll_offset, VISIBLE_ROWS);
                        buzzer_play(BUZZER_EVENT_CLICK);
                    }
                }
                else
                {
                    // Клік по будь-якому іншому рядку: штатно виходимо в CH DETAIL БЕЗ переривання розряду
                    current_state = UI_STATE_CH_DETAIL;
                    selected_param = 0;
                    update_scroll_offset(selected_param, CH_DETAIL_TOTAL_ITEMS, &scroll_offset, VISIBLE_ROWS);
                    buzzer_play(BUZZER_EVENT_CLICK);
                }
                break;
            }
        }

        ui_render(selected_ch, selected_param, scroll_offset, current_state, is_editing, raw_pwm_val);
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