#pragma once

#include <stdint.h>
#include "system_types.h"

// 4-провідна схема Кельвіна виключає опір контактів та дротів
#define R_V_WIRE_MOHM 0

// Максимальна кількість збережених точок деградації DCIR за розряд
#define DCIR_HISTORY_MAX_POINTS 64

// Структура для запису точки DCIR під час розряду
typedef struct
{
    uint32_t timestamp_s;
    uint32_t capacity_mah;
    uint32_t voltage_mv;
    uint32_t dcir_mohm;
} dcir_log_point_t;

// Обнулення локального стану DCIR та історії точок для каналу (при старті/скиданні)
void dcir_service_reset(uint8_t channel);

// Головна функція обробки. Викликається на кожній ітерації ПІД-регулятора.
// Повертає струм, який ПІД має підтримувати прямо зараз (штатний або імпульсний).
uint32_t dcir_service_process(uint8_t channel, channel_metrics_t *metrics, uint32_t target_current_ua);

// Отримання копії історії точок деградації DCIR каналу
uint8_t dcir_service_get_history(uint8_t channel, dcir_log_point_t *out_points, uint8_t max_points);
