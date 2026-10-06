#include "analyzer_service.h"
#include "esp_timer.h"
#include <string.h>

#define TEMP_HISTORY_SEC 60
#define NIMH_DROP_UV 10000    // 10 мВ
#define TEMP_RISE_MC_MIN 1000 // 1°C за хвилину

typedef struct
{
    int32_t temp_history_mc[TEMP_HISTORY_SEC];
    uint8_t head;
    uint64_t last_update_us;
    bool history_filled;
} analyzer_ctx_t;

static analyzer_ctx_t s_ctx[CONFIG_MAX_CHANNELS];

void analyzer_service_reset(uint8_t channel)
{
    if (channel >= CONFIG_MAX_CHANNELS)
        return;
    memset(&s_ctx[channel], 0, sizeof(analyzer_ctx_t));
}

analyzer_verdict_t analyzer_service_evaluate_pre_check(uint8_t channel, channel_metrics_t *metrics)
{
    uint64_t current_time = esp_timer_get_time();

    if (metrics->state_start_time_us == 0)
    {
        metrics->state_start_time_us = current_time;
        return ANALYZER_CONTINUE;
    }

    // Затримка OCV: 30 секунд хімічної релаксації
    if ((current_time - metrics->state_start_time_us) >= 30000000ULL)
    {
        metrics->ocv_uv = metrics->voltage_uv;
        metrics->peak_voltage_uv = metrics->voltage_uv;
        return ANALYZER_FINISHED;
    }
    return ANALYZER_CONTINUE;
}

analyzer_verdict_t analyzer_service_evaluate_discharge(uint8_t channel, channel_metrics_t *metrics)
{
    // 1. Timeout & Capacity Cap (Захист від внутрішнього КЗ)
    if (metrics->settings.time_limit_s > 0 &&
        metrics->elapsed_time_us >= (uint64_t)metrics->settings.time_limit_s * 1000000ULL)
    {
        metrics->error_flags |= ERR_TIMEOUT;
        return ANALYZER_ERROR;
    }

    if (metrics->settings.capacity_limit_mah > 0 &&
        metrics->capacity_mah >= metrics->settings.capacity_limit_mah)
    {
        metrics->error_flags |= ERR_CAPACITY_LIMIT;
        return ANALYZER_ERROR;
    }

    // 2. Voltage Sag (Перевірка у перші 100 мс після старту розряду)
    if (metrics->elapsed_time_us < 100000 && metrics->ocv_uv > 0)
    {
        if ((metrics->ocv_uv - metrics->voltage_uv) > 500000)
        { // Просадка > 500 мВ
            metrics->error_flags |= ERR_VOLTAGE_SAG;
            return ANALYZER_ERROR;
        }
    }

    // 3. Відстеження піку напруги
    if (metrics->voltage_uv > metrics->peak_voltage_uv)
    {
        metrics->peak_voltage_uv = metrics->voltage_uv;
    }

    // 4. Оновлення історії температур та dT/dt (раз на 1 секунду)
    uint64_t current_time = esp_timer_get_time();
    if (current_time - s_ctx[channel].last_update_us >= 1000000ULL)
    {
        s_ctx[channel].last_update_us = current_time;

        int32_t current_temp = 0;
        bool sensor_found = false;

        for (int i = 0; i < MAX_SENSORS_PER_CHANNEL; i++)
        {
            if (metrics->temp_sensors[i].is_bound && metrics->temp_sensors[i].role == SENSOR_ROLE_CELL)
            {
                current_temp = metrics->temp_sensors[i].current_temp_mc;
                sensor_found = true;
                break;
            }
        }

        if (sensor_found)
        {
            int32_t old_temp = s_ctx[channel].temp_history_mc[s_ctx[channel].head];
            s_ctx[channel].temp_history_mc[s_ctx[channel].head] = current_temp;

            s_ctx[channel].head++;
            if (s_ctx[channel].head >= TEMP_HISTORY_SEC)
            {
                s_ctx[channel].head = 0;
                s_ctx[channel].history_filled = true; // Буфер повністю заповнився за хвилину
            }

            // Якщо ми маємо дані хоча б за одну хвилину, обчислюємо dT/dt
            if (s_ctx[channel].history_filled)
            {
                int32_t delta_t = current_temp - old_temp;
                bool is_v_drop = (metrics->peak_voltage_uv > metrics->voltage_uv) &&
                                 ((metrics->peak_voltage_uv - metrics->voltage_uv) >= NIMH_DROP_UV);

                if (metrics->settings.chem == CHEM_NIMH)
                {
                    // КОМПЛЕКСНА УМОВА ДЛЯ NiMH
                    if (is_v_drop && delta_t >= TEMP_RISE_MC_MIN)
                    {
                        return ANALYZER_FINISHED; // Успішне насичення (-dV + dT/dt)
                    }
                    else if (delta_t >= TEMP_RISE_MC_MIN && !is_v_drop)
                    {
                        metrics->error_flags |= ERR_THERMAL_RUNAWAY;
                        return ANALYZER_ERROR; // Термічний розгін (гріється без падіння напруги)
                    }
                    // Якщо тільки is_v_drop без росту температури - ігноруємо (False Peak)
                }
                else
                {
                    // Для літієвих будь-який стрибок 1°C за хвилину є критичним відхиленням
                    if (delta_t >= TEMP_RISE_MC_MIN)
                    {
                        metrics->error_flags |= ERR_THERMAL_RUNAWAY;
                        return ANALYZER_ERROR;
                    }
                }
            }
        }
    }

    return ANALYZER_CONTINUE;
}