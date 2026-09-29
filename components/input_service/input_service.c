#include "input_service.h"
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

static pcnt_unit_handle_t s_pcnt_unit = NULL;
static int s_last_enc_count = 0;
static bool s_btn_prev_state = true; // true = не натиснута (через pull-up)

esp_err_t input_service_init(void)
{
    // Ініціалізація PCNT для енкодера
    pcnt_unit_config_t unit_config = {
        .high_limit = 100,
        .low_limit = -100,
    };
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &s_pcnt_unit));

    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000,
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(s_pcnt_unit, &filter_config));

    pcnt_chan_config_t chan_config = {
        .edge_gpio_num = CONFIG_ENCODER_CLK_PIN,
        .level_gpio_num = CONFIG_ENCODER_DT_PIN,
    };
    pcnt_channel_handle_t pcnt_chan = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(s_pcnt_unit, &chan_config, &pcnt_chan));

    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    ESP_ERROR_CHECK(pcnt_unit_enable(s_pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(s_pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(s_pcnt_unit));

    // Ініціалізація кнопки енкодера
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << CONFIG_ENCODER_SW_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    ESP_ERROR_CHECK(gpio_config(&btn_conf));

    return ESP_OK;
}

void input_service_read(int *enc_diff, bool *enc_btn_clicked)
{
    int enc_count = 0;
    pcnt_unit_get_count(s_pcnt_unit, &enc_count);

    // ВИПРАВЛЕННЯ: Деякі типи енкодерів генерують 2 фронти на 1 фізичний клік.
    // Змінено ділення з 4 на 2, щоб кожен клік відповідав 1 кроку в меню.
    int current_clicks = enc_count / 2;
    int last_clicks = s_last_enc_count / 2;

    *enc_diff = current_clicks - last_clicks;

    if (*enc_diff != 0)
    {
        s_last_enc_count = enc_count;
    }

    // Зчитування та антидебаунс кнопки
    bool btn_curr_state = gpio_get_level(CONFIG_ENCODER_SW_PIN);
    if (s_btn_prev_state && !btn_curr_state)
    {
        *enc_btn_clicked = true;
    }
    else
    {
        *enc_btn_clicked = false;
    }
    s_btn_prev_state = btn_curr_state;
}