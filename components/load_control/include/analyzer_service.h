#pragma once
#include "system_types.h"

typedef enum
{
    ANALYZER_CONTINUE = 0, // Продовжувати роботу
    ANALYZER_FINISHED,     // Штатно завершити стан (тест)
    ANALYZER_ERROR         // Аварія (записано у error_flags)
} analyzer_verdict_t;

void analyzer_service_reset(uint8_t channel);
analyzer_verdict_t analyzer_service_evaluate_pre_check(uint8_t channel, channel_metrics_t *metrics);
analyzer_verdict_t analyzer_service_evaluate_discharge(uint8_t channel, channel_metrics_t *metrics);