#pragma once

#include <stdint.h>
#include "system_types.h"

// Обнулення накопичених значень ємності та енергії при старті нового тесту
void integration_service_reset(channel_metrics_t *metrics);

// Програмне чисельне інтегрування ємності та енергії за поточний крок часу
void integration_service_update(channel_metrics_t *metrics, int64_t dt_us);