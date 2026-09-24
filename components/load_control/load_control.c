#include "load_control.h"
#include "system_state.h"
#include "adc_driver.h"
#include "pwm_driver.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "dcir_service.h"
#include "pid_service.h"
#include "integration_service.h"
#include "sdkconfig.h" // Підключення нових глобальних макросів Kconfig

#ifndef CONFIG_MAX_CHANNELS
#define CONFIG_MAX_CHANNELS 4
#endif

static const char *TAG = "LOAD_CTRL";
static TaskHandle_t s_pid_tasks[CONFIG_MAX_CHANNELS] = {NULL};

// Жорстке відображення логічних каналів на фізичні безпечні піни
static const int pwm_pins[CONFIG_MAX_CHANNELS] = {
    CONFIG_PWM_CH0_PIN,
    CONFIG_PWM_CH1_PIN,
    CONFIG_PWM_CH2_PIN,
    CONFIG_PWM_CH3_PIN};

// Ціль для струму (пізніше винесемо в профілі/команди CLI)
static uint32_t target_current_ua = 1000000; // 1.0 А

// Апаратне керування MOSFET через генерацію V_REF
static void hw_set_load_pwm(uint8_t channel, uint32_t duty)
{
    if (duty == 0)
    {
        load_control_pwm_set(channel, 0);
    }
    else
    {
        load_control_pwm_set(channel, duty);
    }
}

// Функція таски для окремого каналу
static void pid_control_task(void *pvParameters)
{
    uint8_t channel = (uint8_t)((uint32_t)pvParameters);
    channel_metrics_t metrics;

    // Ініціалізація ПІД-регулятора
    pid_context_t channel_pid;
    pid_service_init(&channel_pid, 0.005f, 0.001f, 0.0f, 0.0f, (float)PWM_MAX_DUTY);

    // Використання безпечного піна з конфігураційного масиву
    int channel_pwm_pin = pwm_pins[channel];
    if (load_control_pwm_init(channel, channel_pwm_pin) != ESP_OK)
    {
        ESP_LOGE(TAG, "CH%d: Failed to init PWM on pin %d", channel, channel_pwm_pin);
    }

    int64_t last_time_us = esp_timer_get_time();

    while (1)
    {
        uint32_t notification = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50));

        system_state_get_metrics(channel, &metrics);

        // Реакція на сигнал аварії від Task_Safety
        if (notification > 0)
        {
            ESP_LOGW(TAG, "CH%d: Emergency Abort received! Cutting power.", channel);
            hw_set_load_pwm(channel, 0);

            metrics.state = STATE_ERROR;
            system_state_set_metrics(channel, &metrics);
            continue;
        }

        int64_t current_time_us = esp_timer_get_time();
        int64_t dt_us = current_time_us - last_time_us;
        last_time_us = current_time_us;

        // Обробка кінцевого автомата
        switch (metrics.state)
        {
        case STATE_IDLE:
        case STATE_FINISHED:
        case STATE_ERROR:
            hw_set_load_pwm(channel, 0);
            dcir_service_reset(channel);
            pid_service_reset(&channel_pid);
            integration_service_reset(&metrics);
            break;

        case STATE_PRE_CHECK:
            hw_set_load_pwm(channel, 0);
            dcir_service_reset(channel);
            pid_service_reset(&channel_pid);
            integration_service_reset(&metrics);

            adc_driver_read_voltage(channel, &metrics.voltage_uv);

#ifndef NDEBUG
            if (metrics.voltage_uv > 0)
            {
                metrics.state = STATE_DISCHARGING;
                ESP_LOGI(TAG, "CH%d: [DEBUG] Pre-check passed. Moving to DISCHARGING.", channel);
            }
#else
            if (metrics.voltage_uv >= (CONFIG_MIN_CELL_VOLTAGE_MV * 1000))
            {
                metrics.state = STATE_DISCHARGING;
                ESP_LOGI(TAG, "CH%d: Pre-check passed. Moving to DISCHARGING.", channel);
            }
            else
            {
                metrics.state = STATE_ERROR;
                ESP_LOGE(TAG, "CH%d: Pre-check failed. Voltage too low!", channel);
            }
#endif

            system_state_set_metrics(channel, &metrics);
            break;

        case STATE_DISCHARGING:
            adc_driver_read_voltage(channel, &metrics.voltage_uv);
            adc_driver_read_current(channel, &metrics.current_ua);

            uint32_t active_target_ua = dcir_service_process(channel, &metrics, target_current_ua);

            uint32_t calc_duty = (uint32_t)pid_service_compute(&channel_pid, (float)active_target_ua, (float)metrics.current_ua);
            hw_set_load_pwm(channel, calc_duty);

            integration_service_update(&metrics, dt_us);
            system_state_set_metrics(channel, &metrics);
            break;

        default:
            break;
        }
    }
}

esp_err_t load_control_init(void)
{
    for (uint32_t i = 0; i < CONFIG_MAX_CHANNELS; i++)
    {
        char task_name[16];
        snprintf(task_name, sizeof(task_name), "pid_task_%lu", i);

        BaseType_t res = xTaskCreate(pid_control_task, task_name, 4096, (void *)i, configMAX_PRIORITIES - 2, &s_pid_tasks[i]);
        if (res != pdPASS)
        {
            ESP_LOGE(TAG, "Failed to create %s", task_name);
            return ESP_FAIL;
        }
    }

    ESP_LOGI(TAG, "Load control initialized (%d tasks)", CONFIG_MAX_CHANNELS);
    return ESP_OK;
}

TaskHandle_t load_control_get_task_handle(uint8_t channel)
{
    if (channel >= CONFIG_MAX_CHANNELS)
        return NULL;
    return s_pid_tasks[channel];
}