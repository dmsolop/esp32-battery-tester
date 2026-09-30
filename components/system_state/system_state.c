#include "system_state.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h> // Додано для memset

// Тимчасовий дефолт, поки не додамо його в меню конфігурації (Kconfig)
#ifndef CONFIG_MAX_CHANNELS
#define CONFIG_MAX_CHANNELS 4
#endif

static const char *TAG = "SYSTEM_STATE";

static channel_metrics_t s_channels[CONFIG_MAX_CHANNELS];
static SemaphoreHandle_t s_state_mutex = NULL;

// Матриця безпечних заводських налаштувань для кожної хімії
static const channel_settings_t s_default_settings[3] = {
    [CHEM_LI_ION] = {.chem = CHEM_LI_ION, .target_current_ma = 1000, .cutoff_voltage_mv = 3000, .thermal_limit_mc = 60000, .pro_pid_override = false, .kp = 0.0f, .ki = 0.0f, .kd = 0.0f},
    [CHEM_NIMH] = {.chem = CHEM_NIMH, .target_current_ma = 500, .cutoff_voltage_mv = 1000, .thermal_limit_mc = 50000, .pro_pid_override = false, .kp = 0.0f, .ki = 0.0f, .kd = 0.0f},
    [CHEM_LIFEPO4] = {.chem = CHEM_LIFEPO4, .target_current_ma = 1000, .cutoff_voltage_mv = 2500, .thermal_limit_mc = 50000, .pro_pid_override = false, .kp = 0.0f, .ki = 0.0f, .kd = 0.0f}};

esp_err_t system_state_init(void)
{
    if (s_state_mutex != NULL)
        return ESP_OK;

    s_state_mutex = xSemaphoreCreateMutex();
    if (s_state_mutex == NULL)
    {
        ESP_LOGE(TAG, "Failed to create state mutex");
        return ESP_FAIL;
    }

    // Відкриваємо NVS
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    bool nvs_ok = (err == ESP_OK);

    for (int i = 0; i < CONFIG_MAX_CHANNELS; i++)
    {
        battery_chem_t loaded_chem = CHEM_LI_ION; // Дефолт, якщо запису немає

        if (nvs_ok)
        {
            char key[16];
            snprintf(key, sizeof(key), "ch%d_chem", i);
            uint8_t chem_val = 0;
            if (nvs_get_u8(nvs_handle, key, &chem_val) == ESP_OK && chem_val <= CHEM_LIFEPO4)
            {
                loaded_chem = (battery_chem_t)chem_val;
            }
        }

        // 1. При старті ЗАВЖДИ вантажимо безпечні дефолти обраної хімії
        s_channels[i].settings = s_default_settings[loaded_chem];

        // 2. Ініціалізація датчиків та робочих змінних
        for (int s = 0; s < MAX_SENSORS_PER_CHANNEL; s++)
        {
            memset(s_channels[i].temp_sensors[s].rom, 0, 8);
            s_channels[i].temp_sensors[s].role = SENSOR_ROLE_NONE;
            s_channels[i].temp_sensors[s].current_temp_mc = 0;
            s_channels[i].temp_sensors[s].limit_temp_mc = 0;
            s_channels[i].temp_sensors[s].is_bound = false;
        }

        s_channels[i].voltage_uv = 0;
        s_channels[i].current_ua = 0;
        s_channels[i].pid_current_ua = 0;
        s_channels[i].accumulated_uas = 0;
        s_channels[i].accumulated_uws = 0;
        s_channels[i].capacity_mah = 0;
        s_channels[i].energy_mwh = 0;
        s_channels[i].internal_res_mohm = 0;
        s_channels[i].soh = SOH_UNKNOWN;
        s_channels[i].state = STATE_IDLE;
        s_channels[i].error_flags = 0;

        // Оновлюємо ліміти датчиків після завантаження налаштувань
        system_state_update_sensor_limits(i);
    }

    if (nvs_ok)
        nvs_close(nvs_handle);

    ESP_LOGI(TAG, "System state initialized (%d channels)", CONFIG_MAX_CHANNELS);
    return ESP_OK;
}

esp_err_t system_state_set_metrics(uint8_t channel, const channel_metrics_t *metrics)
{
    if (channel >= CONFIG_MAX_CHANNELS || metrics == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        s_channels[channel] = *metrics;
        xSemaphoreGive(s_state_mutex);
        return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t system_state_get_metrics(uint8_t channel, channel_metrics_t *out_metrics)
{
    if (channel >= CONFIG_MAX_CHANNELS || out_metrics == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        *out_metrics = s_channels[channel];
        xSemaphoreGive(s_state_mutex);
        return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t system_state_set_channel_state(uint8_t channel, channel_state_t state)
{
    if (channel >= CONFIG_MAX_CHANNELS)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        s_channels[channel].state = state;
        xSemaphoreGive(s_state_mutex);
        return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t system_state_get_channel_state(uint8_t channel, channel_state_t *out_state)
{
    if (channel >= CONFIG_MAX_CHANNELS || out_state == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        *out_state = s_channels[channel].state;
        xSemaphoreGive(s_state_mutex);
        return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t system_state_update_sensor_limits(uint8_t channel)
{
    if (channel >= CONFIG_MAX_CHANNELS)
        return ESP_ERR_INVALID_ARG;

    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        for (int s = 0; s < MAX_SENSORS_PER_CHANNEL; s++)
        {
            if (!s_channels[channel].temp_sensors[s].is_bound)
                continue;

            if (s_channels[channel].temp_sensors[s].role == SENSOR_ROLE_HEATSINK)
            {
                s_channels[channel].temp_sensors[s].limit_temp_mc = 85000; // Жорсткий апаратний ліміт
            }
            else if (s_channels[channel].temp_sensors[s].role == SENSOR_ROLE_CELL)
            {
                s_channels[channel].temp_sensors[s].limit_temp_mc = s_channels[channel].settings.thermal_limit_mc;
            }
        }
        xSemaphoreGive(s_state_mutex);
        return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t system_state_set_chemistry(uint8_t channel, battery_chem_t chem)
{
    if (channel >= CONFIG_MAX_CHANNELS || chem > CHEM_LIFEPO4)
        return ESP_ERR_INVALID_ARG;

    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        // Перезаписуємо всі налаштування каналу безпечними дефолтами
        s_channels[channel].settings = s_default_settings[chem];
        xSemaphoreGive(s_state_mutex);

        // Застосовуємо нові температурні ліміти до датчиків
        system_state_update_sensor_limits(channel);

        // Зберігаємо вибір у NVS
        nvs_handle_t nvs_handle;
        if (nvs_open("storage", NVS_READWRITE, &nvs_handle) == ESP_OK)
        {
            char key[16];
            snprintf(key, sizeof(key), "ch%d_chem", channel);
            nvs_set_u8(nvs_handle, key, (uint8_t)chem);
            nvs_commit(nvs_handle);
            nvs_close(nvs_handle);
        }
        return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}