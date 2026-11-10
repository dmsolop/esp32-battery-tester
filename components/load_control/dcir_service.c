#include "dcir_service.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <string.h>

#ifndef CONFIG_MAX_CHANNELS
#define CONFIG_MAX_CHANNELS 4
#endif

static const char *TAG = "DCIR";

// Інтервал між періодичними замірами під час розряду (60 секунд)
#define DCIR_INTERVAL_US (60000000ULL)
// Струм під час імпульсу релаксації (100 мА = 100000 мкА)
#define DCIR_PULSE_CURRENT_UA 100000

typedef struct
{
    uint8_t step;              // 0: штатний розряд, 1: фаза релаксації 450 мс
    int64_t last_trigger_time; // Час останнього успішного заміру
    uint8_t pre_samples;       // Кількість накопичених базових відліків (0..3)
    uint64_t v1_sum_uv;        // Сума 3 відліків напруги перед скиданням струму
    uint64_t i1_sum_ua;        // Сума 3 відліків струму перед скиданням струму
    uint8_t pulse_tick;        // Лічильник тактів (по 50 мс) під час полиці 450 мс (0..8)
    uint64_t v2_sum_uv;        // Сума 3 відліків напруги на хвості полиці (такти 6, 7, 8)
    uint64_t i2_sum_ua;        // Сума 3 відліків струму на хвості полиці (такти 6, 7, 8)
    uint8_t post_samples;      // Кількість відліків на хвості полиці

    dcir_log_point_t history[DCIR_HISTORY_MAX_POINTS];
    uint8_t history_count;
} dcir_state_t;

static dcir_state_t s_dcir_states[CONFIG_MAX_CHANNELS] = {0};

void dcir_service_reset(uint8_t channel)
{
    if (channel >= CONFIG_MAX_CHANNELS)
        return;
    s_dcir_states[channel].step = 0;
    s_dcir_states[channel].last_trigger_time = esp_timer_get_time();
    s_dcir_states[channel].pre_samples = 0;
    s_dcir_states[channel].v1_sum_uv = 0;
    s_dcir_states[channel].i1_sum_ua = 0;
    s_dcir_states[channel].pulse_tick = 0;
    s_dcir_states[channel].v2_sum_uv = 0;
    s_dcir_states[channel].i2_sum_ua = 0;
    s_dcir_states[channel].post_samples = 0;
    s_dcir_states[channel].history_count = 0;
    memset(s_dcir_states[channel].history, 0, sizeof(s_dcir_states[channel].history));
}

uint8_t dcir_service_get_history(uint8_t channel, dcir_log_point_t *out_points, uint8_t max_points)
{
    if (channel >= CONFIG_MAX_CHANNELS || out_points == NULL || max_points == 0)
        return 0;

    uint8_t count = s_dcir_states[channel].history_count;
    if (count > max_points)
        count = max_points;

    memcpy(out_points, s_dcir_states[channel].history, count * sizeof(dcir_log_point_t));
    return count;
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
        // Постійно оновлюємо ковзне вікно з 3 останніх відліків перед стартом імпульсу
        state->v1_sum_uv += metrics->voltage_uv;
        state->i1_sum_ua += metrics->current_ua;
        state->pre_samples++;
        if (state->pre_samples > 3)
        {
            // Тримаємо середнє за 3 останні точки (зрушуємо накопичувач)
            state->v1_sum_uv = (state->v1_sum_uv * 2) / 3 + metrics->voltage_uv;
            state->i1_sum_ua = (state->i1_sum_ua * 2) / 3 + metrics->current_ua;
            state->pre_samples = 3;
        }

        if ((current_time - state->last_trigger_time) >= DCIR_INTERVAL_US && state->pre_samples >= 3)
        {
            // Переходимо до фази імпульсу релаксації (450 мс = 9 тактів по 50 мс)
            state->step = 1;
            state->pulse_tick = 0;
            state->v2_sum_uv = 0;
            state->i2_sum_ua = 0;
            state->post_samples = 0;

            ESP_LOGD(TAG, "CH%d: Dynamic DCIR Pulse Started. Baseline V1=%llu, I1=%llu",
                     channel, state->v1_sum_uv / 3, state->i1_sum_ua / 3);
            return DCIR_PULSE_CURRENT_UA;
        }
        return target_current_ua;
    }

    // Крок 1: Утримання полиці 450 мс (9 тактів по 50 мс)
    if (state->step == 1)
    {
        // Останні 150 мс (такти 6, 7, 8 з 9): накопичення 3 відліків на хвості полиці
        if (state->pulse_tick >= 6)
        {
            state->v2_sum_uv += metrics->voltage_uv;
            state->i2_sum_ua += metrics->current_ua;
            state->post_samples++;
        }

        state->pulse_tick++;

        // Завершення 9-го такту (450 мс)
        if (state->pulse_tick >= 9)
        {
            uint32_t v1_avg = (uint32_t)(state->v1_sum_uv / 3);
            uint32_t i1_avg = (uint32_t)(state->i1_sum_ua / 3);
            uint32_t v2_avg = (state->post_samples > 0) ? (uint32_t)(state->v2_sum_uv / state->post_samples) : metrics->voltage_uv;
            uint32_t i2_avg = (state->post_samples > 0) ? (uint32_t)(state->i2_sum_ua / state->post_samples) : metrics->current_ua;

            // При скиданні струму (I1 > I2) напруга на акумуляторі зростає (V2 > V1)
            if (v2_avg > v1_avg && i1_avg > i2_avg && (i1_avg - i2_avg) >= 150000)
            {
                uint64_t delta_v = (uint64_t)(v2_avg - v1_avg);
                uint64_t delta_i = (uint64_t)(i1_avg - i2_avg);

                // R = (dV * 1000 + dI / 2) / dI (мОм)
                uint32_t dcir_calc = (uint32_t)(((delta_v * 1000ULL) + (delta_i / 2ULL)) / delta_i);
                metrics->internal_res_mohm = dcir_calc;

                // Додавання точки до пулу історії деградації DCIR
                if (state->history_count < DCIR_HISTORY_MAX_POINTS)
                {
                    state->history[state->history_count].timestamp_s = (uint32_t)(metrics->elapsed_time_us / 1000000ULL);
                    state->history[state->history_count].capacity_mah = metrics->capacity_mah;
                    state->history[state->history_count].voltage_mv = metrics->voltage_uv / 1000;
                    state->history[state->history_count].dcir_mohm = dcir_calc;
                    state->history_count++;
                }

                ESP_LOGI(TAG, "CH%d: Dynamic DCIR = %lu mOhm (History points: %u)",
                         channel, dcir_calc, state->history_count);
            }
            else
            {
                ESP_LOGW(TAG, "CH%d: Dynamic DCIR Invalid step (V1=%lu, V2=%lu, I1=%lu, I2=%lu)",
                         channel, v1_avg, v2_avg, i1_avg, i2_avg);
            }

            // Повернення до штатного розряду
            state->last_trigger_time = current_time;
            state->step = 0;
            state->pre_samples = 0;
            state->v1_sum_uv = 0;
            state->i1_sum_ua = 0;
            return target_current_ua;
        }

        // Продовжуємо утримувати імпульс 100 мА
        return DCIR_PULSE_CURRENT_UA;
    }

    return target_current_ua;
}
