#include "integration_service.h"
#include <stddef.h>

void integration_service_reset(channel_metrics_t *metrics)
{
    if (metrics == NULL)
        return;

    metrics->accumulated_uas = 0;
    metrics->accumulated_uws = 0;
    metrics->capacity_mah = 0;
    metrics->energy_mwh = 0;
}

void integration_service_update(channel_metrics_t *metrics, int64_t dt_us)
{
    if (metrics == NULL || dt_us <= 0)
        return;

    // Інтегрування ємності (струм * час)
    metrics->accumulated_uas += (metrics->current_ua * dt_us) / 1000000;

    // Інтегрування енергії (потужність * час)
    uint64_t power_uw = (metrics->voltage_uv / 1000) * (metrics->current_ua / 1000);
    metrics->accumulated_uws += (power_uw * dt_us) / 1000000;

    // Конвертація секундних інтегралів у мА·год та мВт·год
    metrics->capacity_mah = (uint32_t)(metrics->accumulated_uas / 3600000);
    metrics->energy_mwh = (uint32_t)(metrics->accumulated_uws / 3600000);
}