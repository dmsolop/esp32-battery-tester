#include "system_state.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_err.h"
#include <string.h> // Додано для memset

// Тимчасовий дефолт, поки не додамо його в меню конфігурації (Kconfig)
#ifndef CONFIG_MAX_CHANNELS
#define CONFIG_MAX_CHANNELS 4
#endif

static const char *TAG = "SYSTEM_STATE";

static channel_metrics_t s_channels[CONFIG_MAX_CHANNELS];
static SemaphoreHandle_t s_state_mutex = NULL;

esp_err_t system_state_init(void)
{
    if (s_state_mutex != NULL)
    {
        return ESP_OK;
    }

    s_state_mutex = xSemaphoreCreateMutex();
    if (s_state_mutex == NULL)
    {
        ESP_LOGE(TAG, "Failed to create state mutex");
        return ESP_FAIL;
    }

    for (int i = 0; i < CONFIG_MAX_CHANNELS; i++)
    {
        s_channels[i].voltage_uv = 0;
        s_channels[i].current_ua = 0;

        // Ініціалізація всього масиву датчиків для поточного каналу
        for (int s = 0; s < MAX_SENSORS_PER_CHANNEL; s++)
        {
            memset(s_channels[i].temp_sensors[s].rom, 0, 8);
            s_channels[i].temp_sensors[s].role = SENSOR_ROLE_NONE;
            s_channels[i].temp_sensors[s].current_temp_mc = 0;
            s_channels[i].temp_sensors[s].limit_temp_mc = 0;
            s_channels[i].temp_sensors[s].is_bound = false;
        }

        s_channels[i].accumulated_uas = 0;
        s_channels[i].accumulated_uws = 0;
        s_channels[i].capacity_mah = 0;
        s_channels[i].energy_mwh = 0;
        s_channels[i].internal_res_mohm = 0;
        s_channels[i].soh = SOH_UNKNOWN;
        s_channels[i].state = STATE_IDLE;
        s_channels[i].error_flags = 0;
    }

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