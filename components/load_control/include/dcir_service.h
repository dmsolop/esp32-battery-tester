#pragma once

#include <stdint.h>
#include "system_types.h"

// Обнулення локального стану DCIR для каналу (викликається при старті або зупинці тесту)
void dcir_service_reset(uint8_t channel);

// Головна функція обробки. Викликається на кожній ітерації ПІД-регулятора.
// Повертає струм, який ПІД має підтримувати прямо зараз (штатний або імпульсний).
uint32_t dcir_service_process(uint8_t channel, channel_metrics_t *metrics, uint32_t target_current_ua);
