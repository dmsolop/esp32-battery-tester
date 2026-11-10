#include "buzzer_service.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/mcpwm_prelude.h"

static const char *TAG = "BUZZER_SVC";

#ifndef CONFIG_BUZZER_PIN
#define CONFIG_BUZZER_PIN 42
#endif

#define BUZZER_TIMER_RES_HZ 1000000 // 1 MHz resolution (1 us tick)
#define NVS_NAMESPACE "storage"
#define NVS_KEY_VOLUME "bz_vol"

static mcpwm_timer_handle_t s_timer = NULL;
static mcpwm_oper_handle_t s_oper = NULL;
static mcpwm_cmpr_handle_t s_comparator = NULL;
static mcpwm_gen_handle_t s_generator = NULL;

static QueueHandle_t s_buzzer_queue = NULL;
static uint8_t s_volume_pct = 60; // Default 60%

static void buzzer_hw_stop(void)
{
    if (s_comparator)
    {
        mcpwm_comparator_set_compare_value(s_comparator, 0);
    }
}

static void buzzer_hw_play_tone(uint32_t freq_hz)
{
    if (freq_hz == 0 || s_volume_pct == 0 || s_timer == NULL || s_comparator == NULL)
    {
        buzzer_hw_stop();
        return;
    }

    uint32_t period_ticks = BUZZER_TIMER_RES_HZ / freq_hz;
    if (period_ticks < 2)
        period_ticks = 2;

    // Шпаруватість: від 0% до 50% (макс. гучність для меандру = 50% duty)
    // compare_value = period_ticks * (s_volume_pct * 50 / 100) / 100
    uint32_t compare_val = (period_ticks * s_volume_pct) / 200;

    mcpwm_timer_set_period(s_timer, period_ticks);
    mcpwm_comparator_set_compare_value(s_comparator, compare_val);
}

typedef struct
{
    uint32_t freq_hz;
    uint32_t dur_ms;
    uint32_t pause_ms;
} tone_note_t;

static void play_melody(const tone_note_t *notes, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        buzzer_hw_play_tone(notes[i].freq_hz);
        vTaskDelay(pdMS_TO_TICKS(notes[i].dur_ms));
        buzzer_hw_stop();
        if (notes[i].pause_ms > 0)
        {
            vTaskDelay(pdMS_TO_TICKS(notes[i].pause_ms));
        }
    }
}

static void buzzer_task(void *arg)
{
    buzzer_event_t event;
    while (1)
    {
        if (xQueueReceive(s_buzzer_queue, &event, portMAX_DELAY) == pdTRUE)
        {
            switch (event)
            {
            case BUZZER_EVENT_CLICK:
            {
                tone_note_t note = {.freq_hz = 3000, .dur_ms = 15, .pause_ms = 0};
                play_melody(&note, 1);
                break;
            }
            case BUZZER_EVENT_CONFIRM:
            {
                tone_note_t notes[] = {
                    {.freq_hz = 2000, .dur_ms = 50, .pause_ms = 25},
                    {.freq_hz = 3500, .dur_ms = 70, .pause_ms = 0},
                };
                play_melody(notes, sizeof(notes) / sizeof(notes[0]));
                break;
            }
            case BUZZER_EVENT_WARNING:
            {
                tone_note_t notes[] = {
                    {.freq_hz = 1500, .dur_ms = 80, .pause_ms = 50},
                    {.freq_hz = 1500, .dur_ms = 80, .pause_ms = 0},
                };
                play_melody(notes, sizeof(notes) / sizeof(notes[0]));
                break;
            }
            case BUZZER_EVENT_TEST_DONE:
            {
                tone_note_t notes[] = {
                    {.freq_hz = 1800, .dur_ms = 80, .pause_ms = 40},
                    {.freq_hz = 2400, .dur_ms = 80, .pause_ms = 40},
                    {.freq_hz = 3200, .dur_ms = 180, .pause_ms = 0},
                };
                play_melody(notes, sizeof(notes) / sizeof(notes[0]));
                break;
            }
            case BUZZER_EVENT_ERROR:
            {
                tone_note_t notes[] = {
                    {.freq_hz = 800, .dur_ms = 200, .pause_ms = 100},
                    {.freq_hz = 800, .dur_ms = 200, .pause_ms = 100},
                    {.freq_hz = 800, .dur_ms = 350, .pause_ms = 0},
                };
                play_melody(notes, sizeof(notes) / sizeof(notes[0]));
                break;
            }
            default:
                break;
            }
        }
    }
}

esp_err_t buzzer_service_init(void)
{
    // 1. Зчитування гучності з NVS
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK)
    {
        uint8_t saved_vol = 60;
        if (nvs_get_u8(nvs_handle, NVS_KEY_VOLUME, &saved_vol) == ESP_OK)
        {
            if (saved_vol <= 100)
            {
                s_volume_pct = saved_vol;
            }
        }
        nvs_close(nvs_handle);
    }

    ESP_LOGI(TAG, "Initializing MCPWM Buzzer on GPIO %d (Volume: %u%%)", CONFIG_BUZZER_PIN, s_volume_pct);

    // 2. Ініціалізація MCPWM
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = BUZZER_TIMER_RES_HZ,
        .period_ticks = 1000, // 1 kHz початковий період
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    err = mcpwm_new_timer(&timer_config, &s_timer);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create MCPWM timer: %s", esp_err_to_name(err));
        return err;
    }

    mcpwm_operator_config_t oper_config = {
        .group_id = 0,
    };
    err = mcpwm_new_operator(&oper_config, &s_oper);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create MCPWM operator: %s", esp_err_to_name(err));
        return err;
    }

    err = mcpwm_operator_connect_timer(s_oper, s_timer);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to connect timer to operator: %s", esp_err_to_name(err));
        return err;
    }

    mcpwm_comparator_config_t comp_config = {
        .flags.update_cmp_on_tez = true,
    };
    err = mcpwm_new_comparator(s_oper, &comp_config, &s_comparator);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create comparator: %s", esp_err_to_name(err));
        return err;
    }

    mcpwm_generator_config_t gen_config = {
        .gen_gpio_num = CONFIG_BUZZER_PIN,
    };
    err = mcpwm_new_generator(s_oper, &gen_config, &s_generator);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create generator: %s", esp_err_to_name(err));
        return err;
    }

    mcpwm_generator_set_action_on_timer_event(s_generator,
                                              MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH));
    mcpwm_generator_set_action_on_compare_event(s_generator,
                                                MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, s_comparator, MCPWM_GEN_ACTION_LOW));

    // Початково рівень 0
    mcpwm_comparator_set_compare_value(s_comparator, 0);

    err = mcpwm_timer_enable(s_timer);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to enable timer: %s", esp_err_to_name(err));
        return err;
    }

    err = mcpwm_timer_start_stop(s_timer, MCPWM_TIMER_START_NO_STOP);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start timer: %s", esp_err_to_name(err));
        return err;
    }

    // 3. Створення черги та таски
    s_buzzer_queue = xQueueCreate(10, sizeof(buzzer_event_t));
    if (s_buzzer_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create buzzer queue");
        return ESP_ERR_NO_MEM;
    }

    BaseType_t ret = xTaskCreate(buzzer_task, "buzzer_task", 3072, NULL, 4, NULL);
    if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create buzzer task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Buzzer service started successfully");
    return ESP_OK;
}

void buzzer_play(buzzer_event_t event)
{
    if (s_buzzer_queue != NULL)
    {
        xQueueSend(s_buzzer_queue, &event, 0);
    }
}

void buzzer_set_volume(uint8_t volume_pct)
{
    if (volume_pct > 100)
        volume_pct = 100;
    s_volume_pct = volume_pct;

    nvs_handle_t nvs_handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle) == ESP_OK)
    {
        nvs_set_u8(nvs_handle, NVS_KEY_VOLUME, s_volume_pct);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    }
}

uint8_t buzzer_get_volume(void)
{
    return s_volume_pct;
}
