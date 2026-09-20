#pragma once

#include <stdint.h>
#include <stdbool.h>

#define MAX_CHANNELS 4
#define MAX_SENSORS_PER_CHANNEL 3

// Стани кінцевого автомата каналу
typedef enum
{
    STATE_IDLE = 0,        // Очікування підключення / команди
    STATE_PRE_CHECK,       // Перевірка OCV напруги початкового заряду
    STATE_CHARGING,        // Опціональний дозаряд до 100%
    STATE_REL_CALIBRATION, // Тарування/компенсація опору дротів
    STATE_DISCHARGING,     // Активний розряд (CC/CP/CR) + вимірювання DCIR
    STATE_FINISHED,        // Успішне завершення тесту
    STATE_ERROR            // Аварійне зупинення (OTP, OCP, OVP)
} channel_state_t;

// Вердикт стану акумулятора (State of Health)
typedef enum
{
    SOH_UNKNOWN = 0,
    SOH_EXCELLENT, // > 90% ємності, нормований DCIR
    SOH_GOOD,      // 80-90% ємності
    SOH_DEGRADED,  // 60-80% ємності або завищений DCIR
    SOH_DEAD       // < 60% ємності або критичний опір
} soh_verdict_t;

// Ролі температурних датчиків
typedef enum
{
    SENSOR_ROLE_NONE = 0,
    SENSOR_ROLE_CELL_LIION,   // Ліміт 60°C
    SENSOR_ROLE_CELL_LIFEPO4, // Ліміт 50°C
    SENSOR_ROLE_HEATSINK      // Ліміт 85°C
} sensor_role_t;

// Структура окремого термодатчика
typedef struct
{
    uint8_t rom[8];          // Унікальна 64-бітна адреса
    sensor_role_t role;      // Роль датчика у системі
    int32_t current_temp_mc; // Поточна температура у міліградусах (mC)
    int32_t limit_temp_mc;   // Індивідуальний хард-ліміт для цієї ролі (mC)
    bool is_bound;           // Чи прив'язаний цей датчик фізично
} temp_sensor_data_t;

// Структура метрик для одного незалежного каналу
typedef struct
{
    // Виміри реального часу (мікроодиниці)
    uint32_t voltage_uv; // Напруга на щупах Кельвіна (мкВ)
    uint32_t current_ua; // Поточний струм розряду (мкА)

    // Масив термодатчиків для цього каналу
    temp_sensor_data_t temp_sensors[MAX_SENSORS_PER_CHANNEL];

    // Точне чисельне інтегрування
    uint64_t accumulated_uas; // Накопичений заряд (мкА·с)
    uint64_t accumulated_uws; // Накопичена енергія (мкВТ·с)
    uint32_t capacity_mah;    // Ємність для виводу в UI (мА·год)
    uint32_t energy_mwh;      // Енергія для виводу в UI (мВт·год)

    // Внутрішній опір та вердикт
    uint32_t internal_res_mohm; // Обчислений опір DCIR (мОм)
    soh_verdict_t soh;          // Вердикт SOH

    // Статуси та помилки
    channel_state_t state; // Поточний стан автомата
    uint32_t error_flags;  // Бітова маска помилок
} channel_metrics_t;