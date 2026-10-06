#pragma once

#include <stdint.h>
#include <stdbool.h>

#define CONFIG_MAX_CHANNELS 4
#define MAX_SENSORS_PER_CHANNEL 6

// Бітові маски помилок (Глобальні)
#define ERR_OVER_TEMP 0x01
#define ERR_OVER_CURRENT 0x02
#define ERR_OVER_VOLTAGE 0x04
#define ERR_THERMAL_RUNAWAY 0x08
#define ERR_VOLTAGE_SAG 0x10
#define ERR_TIMEOUT 0x20
#define ERR_CAPACITY_LIMIT 0x40
#define ERR_OPEN_CIRCUIT 0x80 // Нова помилка: обрив ланцюга / перегорів запобіжник

// Стани кінцевого автомата каналу
typedef enum
{
    STATE_IDLE = 0,
    STATE_SELF_TEST,
    STATE_PRE_CHECK,
    STATE_CHARGING,
    STATE_DISCHARGING,
    STATE_FINISHED,
    STATE_ERROR
} channel_state_t;

typedef enum
{
    SOH_UNKNOWN = 0,
    SOH_EXCELLENT,
    SOH_GOOD,
    SOH_DEGRADED,
    SOH_DEAD
} soh_verdict_t;
typedef enum
{
    SENSOR_ROLE_NONE = 0,
    SENSOR_ROLE_CELL,
    SENSOR_ROLE_HEATSINK
} sensor_role_t;
typedef enum
{
    CHEM_LI_ION = 0,
    CHEM_NIMH,
    CHEM_LIFEPO4
} battery_chem_t;

typedef struct
{
    uint8_t rom[8];
    sensor_role_t role;
    int32_t current_temp_mc;
    int32_t limit_temp_mc;
    bool is_bound;
} temp_sensor_data_t;

typedef struct
{
    battery_chem_t chem;
    uint32_t target_current_ma;
    uint32_t cutoff_voltage_mv;
    int32_t thermal_limit_mc;

    // Нові ліміти захисту (сторожові таймери та ємність)
    uint32_t capacity_limit_mah;
    uint32_t time_limit_s;

    bool pro_pid_override;
    float kp, ki, kd;
} channel_settings_t;

typedef struct
{
    uint32_t voltage_uv;
    uint32_t current_ua;
    int32_t pid_current_ua;

    // Нові метрики для хімічного аналізу
    uint32_t ocv_uv;              // Істинна напруга спокою
    uint32_t peak_voltage_uv;     // Максимальна зафіксована напруга (-V алгоритм)
    uint64_t state_start_time_us; // Час входу в поточний стан
    uint64_t elapsed_time_us;     // Час, проведений в активному стані

    temp_sensor_data_t temp_sensors[MAX_SENSORS_PER_CHANNEL];
    channel_settings_t settings;

    uint64_t accumulated_uas;
    uint64_t accumulated_uws;
    uint32_t capacity_mah;
    uint32_t energy_mwh;
    uint32_t internal_res_mohm;
    soh_verdict_t soh;

    channel_state_t state;
    uint32_t error_flags;
} channel_metrics_t;