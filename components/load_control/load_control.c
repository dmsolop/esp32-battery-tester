#include "load_control.h"
#include "system_state.h"
#include "adc_driver.h"
#include "pwm_driver.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "dcir_service.h"
#include "pid_service.h"
#include "integration_service.h"
#include "analyzer_service.h"
#include "sdkconfig.h"

#ifndef CONFIG_MAX_CHANNELS
#define CONFIG_MAX_CHANNELS 4
#endif

static const char *TAG = "LOAD_CTRL";
static TaskHandle_t s_pid_tasks[CONFIG_MAX_CHANNELS] = {NULL};
static volatile bool s_manual_override[CONFIG_MAX_CHANNELS] = {false};
static volatile bool s_fan_manual_override[CONFIG_MAX_CHANNELS] = {false};
static volatile uint8_t s_fan_duty_pct[CONFIG_MAX_CHANNELS] = {0};

// Жорстке відображення логічних каналів на фізичні безпечні піни
static const int pwm_pins[CONFIG_MAX_CHANNELS] = {
    CONFIG_PWM_CH0_PIN,
    CONFIG_PWM_CH1_PIN,
    CONFIG_PWM_CH2_PIN,
    CONFIG_PWM_CH3_PIN};

static const int fan_pins[CONFIG_MAX_CHANNELS] = {
    CONFIG_FAN_CH0_PIN,
    CONFIG_FAN_CH1_PIN,
    CONFIG_FAN_CH2_PIN,
    CONFIG_FAN_CH3_PIN};

// Апаратне керування MOSFET через генерацію V_REF
void hw_set_load_pwm(uint8_t channel, uint32_t duty)
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

void load_control_set_raw_pwm(uint8_t channel, uint16_t duty)
{
    if (channel >= CONFIG_MAX_CHANNELS)
    {
        return;
    }

    if (duty > 0)
    {
        s_manual_override[channel] = true;
        hw_set_load_pwm(channel, (uint32_t)duty);
    }
    else
    {
        hw_set_load_pwm(channel, 0);
        s_manual_override[channel] = false;
    }
}

void load_control_set_fan_pwm(uint8_t channel, uint8_t duty_percent)
{
    if (channel >= CONFIG_MAX_CHANNELS)
    {
        return;
    }

    if (duty_percent > 100)
    {
        duty_percent = 100;
    }

    s_fan_duty_pct[channel] = duty_percent;
    s_fan_manual_override[channel] = true;

    // Масштабування 0..100% у 0..4095
    uint32_t raw_duty = ((uint32_t)duty_percent * PWM_MAX_DUTY) / 100;
    load_control_fan_pwm_set(channel, raw_duty);
}

uint8_t load_control_get_fan_pwm(uint8_t channel)
{
    if (channel >= CONFIG_MAX_CHANNELS)
    {
        return 0;
    }
    return s_fan_duty_pct[channel];
}

void load_control_set_fan_manual_override(uint8_t channel, bool manual)
{
    if (channel >= CONFIG_MAX_CHANNELS)
    {
        return;
    }
    s_fan_manual_override[channel] = manual;
}

// Функція таски для окремого каналу
static void pid_control_task(void *pvParameters)
{
    uint8_t channel = (uint8_t)((uint32_t)pvParameters);
    channel_metrics_t metrics;

    pid_context_t channel_pid;
    // Адаптовані коефіцієнти позиційного ПІД під 12-бітний ШІМ (0..4095)
    // та дільник 10:1 (V_ref = V_pwm * 0.1, R_shunt = 0.27 Ом для макету, ~3000 мкА/duty)
    pid_service_init(&channel_pid, 0.002f, 0.005f, 0.0f, 0.0f, (float)PWM_MAX_DUTY);

    int channel_pwm_pin = pwm_pins[channel];
    if (load_control_pwm_init(channel, channel_pwm_pin) != ESP_OK)
    {
        ESP_LOGE(TAG, "CH%d: Failed to init PWM on pin %d", channel, channel_pwm_pin);
    }

    int fan_pwm_pin = fan_pins[channel];
    if (load_control_fan_pwm_init(channel, fan_pwm_pin) != ESP_OK)
    {
        ESP_LOGE(TAG, "CH%d: Failed to init Fan PWM on pin %d", channel, fan_pwm_pin);
    }

    int64_t last_time_us = esp_timer_get_time();

    while (1)
    {
        uint32_t notification = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50));

        system_state_get_metrics(channel, &metrics);

        if (notification > 0)
        {
            ESP_LOGW(TAG, "CH%d: Emergency Abort received! Cutting power.", channel);
            s_manual_override[channel] = false;
            hw_set_load_pwm(channel, 0);

            metrics.state = STATE_ERROR;
            system_state_set_metrics(channel, &metrics);

            continue;
        }

        int64_t current_time_us = esp_timer_get_time();
        int64_t dt_us = current_time_us - last_time_us;
        last_time_us = current_time_us;

        // Автоматичне терморегулювання кулера охолодження (якщо немає ручного перевизначення)
        if (!s_fan_manual_override[channel])
        {
            int32_t max_hs_temp_mc = 0;
            for (int s = 0; s < MAX_SENSORS_PER_CHANNEL; s++)
            {
                if (metrics.temp_sensors[s].is_bound && metrics.temp_sensors[s].role == SENSOR_ROLE_HEATSINK)
                {
                    if (metrics.temp_sensors[s].current_temp_mc > max_hs_temp_mc)
                    {
                        max_hs_temp_mc = metrics.temp_sensors[s].current_temp_mc;
                    }
                }
            }

            uint8_t target_fan_pct = 0;
            if (max_hs_temp_mc < 40000)
            {
                target_fan_pct = 0; // До 40 C кулер вимкнений (тихий режим)
            }
            else if (max_hs_temp_mc >= 75000)
            {
                target_fan_pct = 100; // Від 75 C 100% обертів
            }
            else
            {
                // Від 40 C до 75 C лінійне зростання 30%..100%
                target_fan_pct = 30 + (uint8_t)(((int64_t)(max_hs_temp_mc - 40000) * 70) / (75000 - 40000));
            }

            s_fan_duty_pct[channel] = target_fan_pct;
            uint32_t raw_fan_duty = ((uint32_t)target_fan_pct * PWM_MAX_DUTY) / 100;
            load_control_fan_pwm_set(channel, raw_fan_duty);
        }

        switch (metrics.state)
        {
        case STATE_IDLE:
        case STATE_FINISHED:
        case STATE_CHARGING: // Тимчасова заглушка для проходження компіляції
        case STATE_ERROR:
            if (!s_manual_override[channel])
            {
                hw_set_load_pwm(channel, 0);
            }
            dcir_service_reset(channel);
            pid_service_reset(&channel_pid);
            integration_service_reset(&metrics);
            analyzer_service_reset(channel); // Скидаємо буфери
            break;
        case STATE_SELF_TEST:
            s_manual_override[channel] = false;
            // Крок 1. Валідація термодатчиків (Thermal Sanity)
            bool thermal_ok = true;
            for (int s = 0; s < MAX_SENSORS_PER_CHANNEL; s++)
            {
                if (metrics.temp_sensors[s].is_bound)
                {
                    int32_t temp = metrics.temp_sensors[s].current_temp_mc;
                    // Перевірка на помилки шини 1-Wire (85°C - скидання живлення, -127°C - обрив)
                    if (temp == 85000 || temp <= -100000)
                    {
                        thermal_ok = false;
                        metrics.error_flags |= ERR_OVER_TEMP;
                        break;
                    }
                }
            }

            if (!thermal_ok)
            {
                metrics.state = STATE_ERROR;
                ESP_LOGE(TAG, "CH%d: Thermal Sanity Check FAILED.", channel);
                system_state_set_metrics(channel, &metrics);
                break;
            }

            // Крок 2. Зсув нуля (Zero-Offset)
            hw_set_load_pwm(channel, 0);
            vTaskDelay(pdMS_TO_TICKS(20)); // Очікування розряду паразитних ємностей

            uint32_t zero_current_ua = 0;
            int32_t raw_pid_ua = 0;
            adc_driver_read_current(channel, &zero_current_ua, &raw_pid_ua);

            // Компенсуємо дрейф нуля ОП
            adc_driver_set_zero_offset(channel, 0, raw_pid_ua);
            ESP_LOGI(TAG, "CH%d: Zero offset calibrated: %ld uA", channel, raw_pid_ua);

            // Крок 3. Перевірка силового ланцюга (Power Path Check)
            // Даємо тестовий ШІМ (~10% від 4095: duty=400 створює V_ref ~32 мВ після дільника 10:1;
            // при шунті 0.27 Ом очікуваний струм становить ~119 мА, при 5 мОм > 1 А).
            hw_set_load_pwm(channel, 400); // 400 відліків для 12-бітного ШІМ

            // Враховуємо динаміку:
            // 1) Дволанковий ФНЧ Б2 (R1=10k, R2=10k, C1=100nF, C2=100nF) має тау ~1.5..2 мс, встановлення > 10..15 мс.
            // 2) АЦП ADS1115 (128 SPS) має період перетворення ~7.8 мс.
            // 3) Хімічна та індуктивна релаксація силового контуру.
            // Використовуємо циклічне опитування з таймаутом 200 мс (затримка кроку 20 мс) до появи ненульового струму.
            uint32_t ping_current_ua = 0;
            for (int poll = 0; poll < 10; poll++)
            {
                vTaskDelay(pdMS_TO_TICKS(20)); // Сумарно до 200 мс очікування встановлення
                adc_driver_read_current(channel, &ping_current_ua, &raw_pid_ua);
                if (ping_current_ua >= 10000) // Поріг детекції струму: 10 мА (захист від шуму ОП при очікуваних >100 мА)
                {
                    break;
                }
            }

            ESP_LOGW(TAG, "DEBUG: Ping current = %lu uA (raw = %ld)", ping_current_ua, raw_pid_ua);
            hw_set_load_pwm(channel, 0); // Миттєво закриваємо транзистор

            if (ping_current_ua < 10000)
            { // Якщо струм менше 10 мА - фізичний обрив кола / вимкнене джерело / спрацював запобіжник
                metrics.state = STATE_ERROR;
                metrics.error_flags |= ERR_OPEN_CIRCUIT;
                ESP_LOGE(TAG, "CH%d: Power Path Check FAILED (Open Circuit: I=%lu uA < 10mA).", channel, ping_current_ua);
            }
            else
            {
                metrics.state = STATE_PRE_CHECK;
                metrics.state_start_time_us = 0; // Скидаємо таймер для коректного старту OCV паузи
                ESP_LOGI(TAG, "CH%d: Self-Test PASSED (Ping I=%lu uA). Moving to PRE_CHECK.", channel, ping_current_ua);
            }

            system_state_set_metrics(channel, &metrics);
            break;

        case STATE_PRE_CHECK:
            s_manual_override[channel] = false;
            hw_set_load_pwm(channel, 0);
            adc_driver_read_voltage(channel, &metrics.voltage_uv);

            // Виклик бізнес-логіки діагностики OCV
            analyzer_verdict_t pre_verdict = analyzer_service_evaluate_pre_check(channel, &metrics);

            if (pre_verdict == ANALYZER_FINISHED)
            {
                // Перевірка порогу розряду перед стартом
                uint32_t start_threshold_uv = (metrics.settings.cutoff_voltage_mv * 1000) + 100000;

                if (metrics.ocv_uv >= start_threshold_uv && metrics.ocv_uv >= (CONFIG_MIN_CELL_VOLTAGE_MV * 1000))
                {
                    metrics.state = STATE_DISCHARGING;
                    metrics.state_start_time_us = esp_timer_get_time();
                    metrics.elapsed_time_us = 0;
                    ESP_LOGI(TAG, "CH%d: OCV %.2fV. Moving to DISCHARGING.", channel, metrics.ocv_uv / 1000000.0f);
                }
                else
                {
                    metrics.state = STATE_ERROR;
                    ESP_LOGE(TAG, "CH%d: OCV too low for start.", channel);
                }
            }
            system_state_set_metrics(channel, &metrics);
            break;

        case STATE_DISCHARGING:
            s_manual_override[channel] = false;
            metrics.elapsed_time_us += dt_us; // Накопичуємо час

            adc_driver_read_voltage(channel, &metrics.voltage_uv);
            adc_driver_read_current(channel, &metrics.current_ua, &metrics.pid_current_ua);

            integration_service_update(&metrics, dt_us);

            // Виклик бізнес-логіки FSM
            analyzer_verdict_t dis_verdict = analyzer_service_evaluate_discharge(channel, &metrics);

            if (dis_verdict == ANALYZER_FINISHED)
            {
                hw_set_load_pwm(channel, 0);
                metrics.state = STATE_FINISHED;
                ESP_LOGI(TAG, "CH%d: Test FINISHED (Condition met).", channel);
            }
            else if (dis_verdict == ANALYZER_ERROR)
            {
                hw_set_load_pwm(channel, 0);
                metrics.state = STATE_ERROR;
                ESP_LOGE(TAG, "CH%d: ANALYZER ERROR. Mask: 0x%02lX", channel, metrics.error_flags);
            }
            else
            {
                // Нормальне керування ПІД
                uint32_t target_ua = metrics.settings.target_current_ma * 1000;

                if (metrics.settings.pro_pid_override)
                {
                    pid_service_init(&channel_pid, metrics.settings.kp, metrics.settings.ki, metrics.settings.kd, 0.0f, (float)PWM_MAX_DUTY);
                }

                uint32_t active_target_ua = dcir_service_process(channel, &metrics, target_ua);
                float dt_sec = (float)dt_us / 1000000.0f;
                uint32_t calc_duty = (uint32_t)pid_service_compute(&channel_pid, (float)active_target_ua, (float)metrics.pid_current_ua, dt_sec);
                hw_set_load_pwm(channel, calc_duty);

                // Класична відсічка по напрузі (CC/CP розряд)
                uint32_t cutoff_uv = metrics.settings.cutoff_voltage_mv * 1000;
                if (metrics.voltage_uv <= cutoff_uv && metrics.voltage_uv > 0)
                {
                    hw_set_load_pwm(channel, 0);
                    metrics.state = STATE_FINISHED;
                    ESP_LOGI(TAG, "CH%d: Cutoff voltage reached.", channel);
                }
            }

            system_state_set_metrics(channel, &metrics);
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