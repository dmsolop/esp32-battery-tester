#include "ui_interface.h"
#include "display_engine.h"
#include "icons.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include <stdio.h>

// Машина станів нашого інтерфейсу
typedef enum
{
    UI_STATE_CH_LIST,    // Фокус на лівому екрані (вибір каналу)
    UI_STATE_PARAM_LIST, // Фокус на правому екрані (вибір параметра)
    UI_STATE_GRAPH       // Фокус на правому екрані (перегляд графіка)
} ui_state_t;

static pcnt_unit_handle_t s_pcnt_unit = NULL;

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

        // Курсор малюється тут тільки якщо ми в режимі вибору каналу
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
        // Рендеримо макет графіка
        display_engine_draw_string(rx + 4, 4, "--- GRAPH VIEW ---", 1);
        display_engine_draw_string(rx + 10, 30, "[ GRAPH RENDERING ]", 1);
        display_engine_draw_string(rx + 20, 50, "Click to Exit", 1);
    }
    else
    {
        // Рендеримо меню параметрів
        char title_str[24];
        snprintf(title_str, sizeof(title_str), "--- CH %d DETAIL ---", selected_ch);
        display_engine_draw_string(rx + 4, 4, title_str, 1);

        const char *params[] = {"MODE: DISCHARGE", "CAP:  1250 mAh", "RES:  45 mOhm", "[ <- BACK ]"};

        for (int i = 0; i < 4; i++)
        {
            int y_pos = 20 + (i * 12);
            // Курсор малюється тут тільки якщо ми в режимі вибору параметра
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

    int last_enc_count = 0;
    bool btn_prev_state = true; // true = не натиснута (підтяжка)

    while (1)
    {
        // 1. Зчитування енкодера
        int enc_count = 0;
        pcnt_unit_get_count(s_pcnt_unit, &enc_count);

        // Стандартний EC11 генерує 4 фронти на один фізичний клік
        int current_clicks = enc_count / 4;
        int last_clicks = last_enc_count / 4;
        int diff = current_clicks - last_clicks;

        if (diff != 0)
        {
            if (current_state == UI_STATE_CH_LIST)
            {
                selected_ch = (selected_ch + diff) % 4;
                if (selected_ch < 0)
                    selected_ch += 4; // Захист від від'ємного залишку
            }
            else if (current_state == UI_STATE_PARAM_LIST)
            {
                selected_param = (selected_param + diff) % 4;
                if (selected_param < 0)
                    selected_param += 4;
            }
            last_enc_count = enc_count;
        }

        // 2. Зчитування кнопки з антибрязкотом (програмним, на базі затримки таски)
        bool btn_curr_state = gpio_get_level(CONFIG_ENCODER_SW_PIN);
        if (btn_prev_state && !btn_curr_state)
        { // Ловимо момент натискання

            if (current_state == UI_STATE_CH_LIST)
            {
                current_state = UI_STATE_PARAM_LIST;
                selected_param = 0; // Скидаємо фокус на верхній параметр
            }
            else if (current_state == UI_STATE_PARAM_LIST)
            {
                if (selected_param == 3)
                {
                    current_state = UI_STATE_CH_LIST; // Повернення назад
                }
                else
                {
                    current_state = UI_STATE_GRAPH; // Перехід до графіка
                }
            }
            else if (current_state == UI_STATE_GRAPH)
            {
                current_state = UI_STATE_PARAM_LIST; // Вихід з графіка кліком
            }
        }
        btn_prev_state = btn_curr_state;

        // 3. Відмальовування
        ui_render_dashboard(selected_ch, selected_param, current_state);
        display_engine_update();

        // 50 мс затримка створює UI з частотою 20 FPS і слугує програмним фільтром для кнопки
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void encoder_init(void)
{
    // Ініціалізація PCNT для пінів A та B
    pcnt_unit_config_t unit_config = {
        .high_limit = 100,
        .low_limit = -100,
    };
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &s_pcnt_unit));

    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000, // Фільтрація брязкоту < 1 мкс
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(s_pcnt_unit, &filter_config));

    pcnt_chan_config_t chan_config = {
        .edge_gpio_num = CONFIG_ENCODER_CLK_PIN,
        .level_gpio_num = CONFIG_ENCODER_DT_PIN,
    };
    pcnt_channel_handle_t pcnt_chan = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(s_pcnt_unit, &chan_config, &pcnt_chan));

    // Налаштування квадратурної логіки
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    ESP_ERROR_CHECK(pcnt_unit_enable(s_pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(s_pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(s_pcnt_unit));

    // Ініціалізація кнопки
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << CONFIG_ENCODER_SW_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, // Дублюємо зовнішню підтяжку внутрішньою
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE // Опитуємо кнопку в UI-тасці, переривання не потрібні
    };
    gpio_config(&btn_conf);
}

esp_err_t ui_interface_init(void)
{
    ESP_ERROR_CHECK(display_engine_init());
    encoder_init();

    xTaskCreate(ui_task, "ui_task", 4096, NULL, configMAX_PRIORITIES - 3, NULL);
    return ESP_OK;
}