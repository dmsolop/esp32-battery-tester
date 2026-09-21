#include "dcir_service.h"
#include "esp_timer.h"
#include "esp_log.h"

#ifndef CONFIG_MAX_CHANNELS
#define CONFIG_MAX_CHANNELS 4
#endif

static const char *TAG = "DCIR";

// Інтервал між замірами (60 секунд)
#define DCIR_INTERVAL_US (60000000ULL)
// Тривалість імпульсу просадки (100 мс)
#define DCIR_PULSE_DURATION_US (100000ULL)
// Струм під час імпульсу (100 мА = 100000 мкА)
#define DCIR_PULSE_CURRENT_UA 100000

typedef struct
{
    uint8_t step;              // 0: Штатний розряд, 1: Утримання імпульсу
    int64_t last_trigger_time; // Час останнього успішного заміру
    int64_t pulse_start_time;  // Час початку поточного імпульсу
    uint32_t v1_uv;            // Базова напруга до імпульсу
    uint32_t i1_ua;            // Базовий струм до імпульсу
} dcir_state_t;

static dcir_state_t s_dcir_states[CONFIG_MAX_CHANNELS] = {0};

void dcir_service_reset(uint8_t channel)
{
    if (channel >= CONFIG_MAX_CHANNELS)
        return;
    s_dcir_states[channel].step = 0;
    s_dcir_states[channel].last_trigger_time = esp_timer_get_time();
    s_dcir_states[channel].pulse_start_time = 0;
    s_dcir_states[channel].v1_uv = 0;
    s_dcir_states[channel].i1_ua = 0;
}

uint32_t dcir_service_process(uint8_t channel, channel_metrics_t *metrics, uint32_t target_current_ua)
{
    if (channel >= CONFIG_MAX_CHANNELS || metrics == NULL)
        return target_current_ua;

    dcir_state_t *state = &s_dcir_states[channel];
    int64_t current_time = esp_timer_get_time();

    // Крок 0: Штатний розряд та очікування 60 секунд
    if (state->step == 0)
    {
        if ((current_time - state->last_trigger_time) >= DCIR_INTERVAL_US)
        {
            // Фіксуємо базу перед скиданням струму
            state->v1_uv = metrics->voltage_uv;
            state->i1_ua = metrics->current_ua;

            state->pulse_start_time = current_time;
            state->step = 1;

            ESP_LOGD(TAG, "CH%d: DCIR Pulse Started. V1=%lu, I1=%lu", channel, state->v1_uv, state->i1_ua);
            return DCIR_PULSE_CURRENT_UA; // Повертаємо занижений струм для ПІД
        }
        return target_current_ua; // Час не настав, продовжуємо штатний розряд
    }

    // Крок 1: Утримання імпульсу 100 мс та розрахунок
    if (state->step == 1)
    {
        if ((current_time - state->pulse_start_time) >= DCIR_PULSE_DURATION_US)
        {
            // Фіксуємо показники під час просадки (V2, I2)
            uint32_t v2_uv = metrics->voltage_uv;
            uint32_t i2_ua = metrics->current_ua;

            // Захист від ділення на нуль або аномальних показників (V2 має бути більшим за V1 при скиданні струму)
            if (v2_uv > state->v1_uv && state->i1_ua > i2_ua)
            {
                uint32_t delta_v_uv = v2_uv - state->v1_uv;
                uint32_t delta_i_ua = state->i1_ua - i2_ua;

                // Розрахунок R = dV / dI.
                // Множимо на 1000, щоб перевести результат у міліоми (мОм)
                metrics->internal_res_mohm = (delta_v_uv * 1000) / delta_i_ua;

                ESP_LOGI(TAG, "CH%d: DCIR Calculated: %lu mOhm", channel, metrics->internal_res_mohm);
            }
            else
            {
                ESP_LOGW(TAG, "CH%d: DCIR Invalid math (V2<=V1 or I1<=I2). Skipped.", channel);
            }

            // Повернення до штатного стану
            state->last_trigger_time = current_time;
            state->step = 0;
            return target_current_ua;
        }
        // Імпульс ще триває, продовжуємо утримувати 100 мА
        return DCIR_PULSE_CURRENT_UA;
    }

    return target_current_ua;
}
