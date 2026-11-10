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
#include "buzzer_service.h"
#include "sdkconfig.h"

#ifndef CONFIG_MAX_CHANNELS
#define CONFIG_MAX_CHANNELS 4
#endif

static const char *TAG = "LOAD_CTRL";
static TaskHandle_t s_pid_tasks[CONFIG_MAX_CHANNELS] = {NULL};
static volatile bool s_manual_override[CONFIG_MAX_CHANNELS] = {false};
static volatile bool s_diag_self_test_only[CONFIG_MAX_CHANNELS] = {false};
static volatile bool s_diag_precheck_only[CONFIG_MAX_CHANNELS] = {false};
static volatile bool s_req_zero_calib[CONFIG_MAX_CHANNELS] = {false};
static volatile bool s_fan_manual_override[CONFIG_MAX_CHANNELS] = {false};
static volatile uint8_t s_fan_duty_pct[CONFIG_MAX_CHANNELS] = {0};

// DCIR Multi-pulse state tracking
typedef struct
{
    uint8_t cycle;         // 0..9 (10 cycles)
    uint8_t tick_in_cycle; // 0..19 (20 ticks of 50ms = 1000ms)
    uint64_t v1_sum_uv;
    uint64_t i1_sum_ua;
    uint64_t v2_sum_uv;
    uint64_t i2_sum_ua;
    uint32_t samples[10];
    uint8_t sample_count;
} dcir_test_fsm_t;

static dcir_test_fsm_t s_dcir_fsm[CONFIG_MAX_CHANNELS];

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

void load_control_stop(uint8_t channel)
{
    if (channel >= CONFIG_MAX_CHANNELS)
    {
        return;
    }
    s_manual_override[channel] = false;
    s_diag_self_test_only[channel] = false;
    s_diag_precheck_only[channel] = false;
    s_req_zero_calib[channel] = false;
    hw_set_load_pwm(channel, 0);
    system_state_set_channel_state(channel, STATE_IDLE);
    ESP_LOGI(TAG, "CH%d: Stopped immediately by load_control_stop.", channel);
}

void load_control_start_diag_precheck(uint8_t channel)
{
    if (channel >= CONFIG_MAX_CHANNELS)
    {
        return;
    }
    s_manual_override[channel] = false;
    s_diag_self_test_only[channel] = false;
    s_diag_precheck_only[channel] = true;
    s_req_zero_calib[channel] = false;
    hw_set_load_pwm(channel, 0);

    channel_metrics_t m;
    system_state_get_metrics(channel, &m);
    m.state = STATE_PRE_CHECK;
    m.state_start_time_us = 0;
    system_state_set_metrics(channel, &m);
    ESP_LOGI(TAG, "CH%d: Diag Pre-Check OCV started.", channel);
}

void load_control_start_diag_self_test(uint8_t channel)
{
    if (channel >= CONFIG_MAX_CHANNELS)
    {
        return;
    }
    s_manual_override[channel] = false;
    s_diag_precheck_only[channel] = false;
    s_diag_self_test_only[channel] = true;
    s_req_zero_calib[channel] = false;
    hw_set_load_pwm(channel, 0);

    channel_metrics_t m;
    system_state_get_metrics(channel, &m);
    m.state = STATE_SELF_TEST;
    m.state_start_time_us = 0;
    system_state_set_metrics(channel, &m);
    ESP_LOGI(TAG, "CH%d: Diag HW Self-Test started.", channel);
}

void load_control_request_zero_calib(uint8_t channel)
{
    if (channel >= CONFIG_MAX_CHANNELS)
    {
        return;
    }
    s_req_zero_calib[channel] = true;
    ESP_LOGI(TAG, "CH%d: Zero offset calibration requested.", channel);
}

void load_control_start_dcir_test(uint8_t channel)
{
    if (channel >= CONFIG_MAX_CHANNELS)
    {
        return;
    }
    s_manual_override[channel] = false;
    s_diag_precheck_only[channel] = false;
    s_diag_self_test_only[channel] = false;
    s_req_zero_calib[channel] = false;

    memset(&s_dcir_fsm[channel], 0, sizeof(dcir_test_fsm_t));

    channel_metrics_t m;
    system_state_get_metrics(channel, &m);
    m.internal_res_mohm = 0;
    m.state = STATE_DCIR_TEST;
    m.state_start_time_us = esp_timer_get_time();
    system_state_set_metrics(channel, &m);

    buzzer_play(BUZZER_EVENT_CONFIRM);
    ESP_LOGI(TAG, "CH%d: 10-pulse DCIR test started.", channel);
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
            if (s_req_zero_calib[channel])
            {
                s_req_zero_calib[channel] = false;
                hw_set_load_pwm(channel, 0);
                vTaskDelay(pdMS_TO_TICKS(20));

                uint32_t zero_current_ua = 0;
                int32_t raw_pid_ua = 0;
                adc_driver_read_current(channel, &zero_current_ua, &raw_pid_ua);
                adc_driver_set_zero_offset(channel, 0, raw_pid_ua);
                buzzer_play(BUZZER_EVENT_CONFIRM);
                ESP_LOGI(TAG, "CH%d: Zero offset calibrated asynchronously: %ld uA", channel, raw_pid_ua);
            }
            s_diag_self_test_only[channel] = false;
            s_diag_precheck_only[channel] = false;
            if (!s_manual_override[channel])
            {
                hw_set_load_pwm(channel, 0);
            }
            dcir_service_reset(channel);
            pid_service_reset(&channel_pid);
            integration_service_reset(&metrics);
            analyzer_service_reset(channel); // Скидаємо буфери
            break;

        case STATE_FINISHED:
        case STATE_CHARGING: // Тимчасова заглушка для проходження компіляції
        case STATE_ERROR:
            s_diag_self_test_only[channel] = false;
            s_diag_precheck_only[channel] = false;
            if (!s_manual_override[channel])
            {
                hw_set_load_pwm(channel, 0);
            }
            dcir_service_reset(channel);
            pid_service_reset(&channel_pid);
            integration_service_reset(&metrics);
            analyzer_service_reset(channel); // Скидаємо буфери
            break;

        case STATE_DCIR_TEST:
        {
            s_manual_override[channel] = false;
            dcir_test_fsm_t *fsm = &s_dcir_fsm[channel];

            adc_driver_read_voltage(channel, &metrics.voltage_uv);
            adc_driver_read_current(channel, &metrics.current_ua, &metrics.pid_current_ua);

            // Аварійна зупинка: обрив або падіння напруги нижче 500 мВ (500 000 мкВ)
            if (metrics.voltage_uv < 500000)
            {
                hw_set_load_pwm(channel, 0);
                pid_service_reset(&channel_pid);
                metrics.state = STATE_ERROR;
                metrics.error_flags |= ERR_OPEN_CIRCUIT;
                buzzer_play(BUZZER_EVENT_ERROR);
                ESP_LOGE(TAG, "CH%d: DCIR Test ABORTED! Open Circuit (V=%lu uV < 500mV)",
                         channel, metrics.voltage_uv);
                system_state_set_metrics(channel, &metrics);
                break;
            }

            float dt_sec = (float)dt_us / 1000000.0f;
            uint32_t active_target_ua = 0;

            // Цикл k: 20 тактів по 50 мс = 1000 мс
            // Такти 0..9: Low Plateau (100 мА)
            if (fsm->tick_in_cycle < 10)
            {
                active_target_ua = 100000; // 100 мА
                if (fsm->tick_in_cycle == 0)
                {
                    fsm->v1_sum_uv = 0;
                    fsm->i1_sum_ua = 0;
                }
                // Накопичення на тактах 7, 8, 9 (350..500 мс)
                if (fsm->tick_in_cycle >= 7)
                {
                    fsm->v1_sum_uv += metrics.voltage_uv;
                    fsm->i1_sum_ua += metrics.current_ua;
                }
            }
            // Такти 10..19: High Plateau (1000 мА)
            else
            {
                active_target_ua = 1000000; // 1000 мА
                if (fsm->tick_in_cycle == 10)
                {
                    fsm->v2_sum_uv = 0;
                    fsm->i2_sum_ua = 0;
                }
                // Накопичення на тактах 17, 18, 19 (850..1000 мс)
                if (fsm->tick_in_cycle >= 17)
                {
                    fsm->v2_sum_uv += metrics.voltage_uv;
                    fsm->i2_sum_ua += metrics.current_ua;
                }
            }

            // Розрахунок ШІМ через ПІД
            if (metrics.settings.pro_pid_override)
            {
                pid_service_init(&channel_pid, metrics.settings.kp, metrics.settings.ki, metrics.settings.kd, 0.0f, (float)PWM_MAX_DUTY);
            }
            uint32_t calc_duty = (uint32_t)pid_service_compute(&channel_pid, (float)active_target_ua, (float)metrics.pid_current_ua, dt_sec);
            hw_set_load_pwm(channel, calc_duty);

            // Кінець поточного циклу k (такт 19)
            if (fsm->tick_in_cycle == 19)
            {
                uint32_t v1_avg = (uint32_t)(fsm->v1_sum_uv / 3);
                uint32_t i1_avg = (uint32_t)(fsm->i1_sum_ua / 3);
                uint32_t v2_avg = (uint32_t)(fsm->v2_sum_uv / 3);
                uint32_t i2_avg = (uint32_t)(fsm->i2_sum_ua / 3);

                if (fsm->cycle == 0)
                {
                    metrics.ocv_uv = v1_avg;
                }

                if (v1_avg > v2_avg && i2_avg > i1_avg && (i2_avg - i1_avg) >= 200000)
                {
                    uint64_t delta_v = (uint64_t)(v1_avg - v2_avg);
                    uint64_t delta_i = (uint64_t)(i2_avg - i1_avg);
                    uint32_t r_k_mohm = (uint32_t)(((delta_v * 1000ULL) + (delta_i / 2ULL)) / delta_i);

                    if (fsm->cycle < 10)
                    {
                        fsm->samples[fsm->sample_count++] = r_k_mohm;
                    }

                    // Проміжне середнє для відображення в реальному часі на Live Monitor
                    uint32_t running_sum = 0;
                    for (uint8_t s = 0; s < fsm->sample_count; s++)
                    {
                        running_sum += fsm->samples[s];
                    }
                    metrics.internal_res_mohm = running_sum / fsm->sample_count;
                    ESP_LOGI(TAG, "CH%d: DCIR Pulse %u/10: R=%lu mOhm (V1=%lu, V2=%lu, dI=%lu uA)",
                             channel, fsm->cycle + 1, r_k_mohm, v1_avg, v2_avg, (uint32_t)delta_i);
                }
                else
                {
                    ESP_LOGW(TAG, "CH%d: DCIR Pulse %u/10 invalid step: V1=%lu, V2=%lu, I1=%lu, I2=%lu",
                             channel, fsm->cycle + 1, v1_avg, v2_avg, i1_avg, i2_avg);
                }

                // Перевірка завершення 10-го циклу (cycle = 9)
                if (fsm->cycle >= 9)
                {
                    hw_set_load_pwm(channel, 0);
                    pid_service_reset(&channel_pid);

                    if (fsm->sample_count >= 3)
                    {
                        // Застосування Trimmed Mean: відкидаємо min та max
                        uint32_t min_r = fsm->samples[0];
                        uint32_t max_r = fsm->samples[0];
                        uint64_t sum_r = 0;

                        for (uint8_t s = 0; s < fsm->sample_count; s++)
                        {
                            if (fsm->samples[s] < min_r)
                                min_r = fsm->samples[s];
                            if (fsm->samples[s] > max_r)
                                max_r = fsm->samples[s];
                            sum_r += fsm->samples[s];
                        }

                        uint32_t final_dcir_mohm = (uint32_t)((sum_r - min_r - max_r) / (fsm->sample_count - 2));
                        metrics.internal_res_mohm = final_dcir_mohm;

                        // Оцінка SOH
                        if (final_dcir_mohm < 100)
                        {
                            metrics.soh = SOH_GOOD;
                        }
                        else if (final_dcir_mohm <= 250)
                        {
                            metrics.soh = SOH_DEGRADED;
                        }
                        else
                        {
                            metrics.soh = SOH_DEAD;
                        }

                        ESP_LOGI(TAG, "CH%d: 10-Pulse DCIR Complete! Final R=%lu mOhm (Trimmed Mean), SOH=%d",
                                 channel, final_dcir_mohm, metrics.soh);
                        metrics.state = STATE_FINISHED;
                        buzzer_play(BUZZER_EVENT_TEST_DONE);
                    }
                    else
                    {
                        ESP_LOGE(TAG, "CH%d: DCIR test failed (not enough valid samples: %u)",
                                 channel, fsm->sample_count);
                        metrics.state = STATE_ERROR;
                        metrics.error_flags |= ERR_VOLTAGE_SAG;
                        buzzer_play(BUZZER_EVENT_ERROR);
                    }

                    system_state_set_metrics(channel, &metrics);
                    break;
                }

                fsm->cycle++;
                fsm->tick_in_cycle = 0;
            }
            else
            {
                fsm->tick_in_cycle++;
            }

            system_state_set_metrics(channel, &metrics);
            break;
        }
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
                s_diag_self_test_only[channel] = false;
                metrics.state = STATE_ERROR;
                metrics.error_flags |= ERR_OPEN_CIRCUIT;
                buzzer_play(BUZZER_EVENT_ERROR);
                ESP_LOGE(TAG, "CH%d: Power Path Check FAILED (Open Circuit: I=%lu uA < 10mA).", channel, ping_current_ua);
            }
            else
            {
                if (s_diag_self_test_only[channel])
                {
                    s_diag_self_test_only[channel] = false;
                    metrics.state = STATE_IDLE;
                    buzzer_play(BUZZER_EVENT_TEST_DONE);
                    ESP_LOGI(TAG, "CH%d: Diag Self-Test PASSED (Ping I=%lu uA). Returning to IDLE.", channel, ping_current_ua);
                }
                else
                {
                    metrics.state = STATE_PRE_CHECK;
                    metrics.state_start_time_us = 0; // Скидаємо таймер для коректного старту OCV паузи
                    ESP_LOGI(TAG, "CH%d: Self-Test PASSED (Ping I=%lu uA). Moving to PRE_CHECK.", channel, ping_current_ua);
                }
            }

            system_state_set_metrics(channel, &metrics);
            break;

        case STATE_PRE_CHECK:
            s_manual_override[channel] = false;
            hw_set_load_pwm(channel, 0);
            adc_driver_read_voltage(channel, &metrics.voltage_uv);

            // Якщо це діагностичний запуск [ Pre-Check OCV ], негайно фіксуємо OCV і повертаємося в STATE_IDLE
            if (s_diag_precheck_only[channel])
            {
                s_diag_precheck_only[channel] = false;
                metrics.ocv_uv = metrics.voltage_uv;
                metrics.peak_voltage_uv = metrics.voltage_uv;

                if (metrics.ocv_uv >= (CONFIG_MIN_CELL_VOLTAGE_MV * 1000))
                {
                    metrics.state = STATE_IDLE;
                    buzzer_play(BUZZER_EVENT_TEST_DONE);
                    ESP_LOGI(TAG, "CH%d: Diag Pre-Check OCV: %.3f V. Returning to IDLE safely.",
                             channel, metrics.ocv_uv / 1000000.0f);
                }
                else
                {
                    metrics.state = STATE_ERROR;
                    buzzer_play(BUZZER_EVENT_ERROR);
                    ESP_LOGE(TAG, "CH%d: Diag Pre-Check OCV too low (%.3f V).",
                             channel, metrics.ocv_uv / 1000000.0f);
                }
                system_state_set_metrics(channel, &metrics);
                break;
            }

            // Виклик бізнес-логіки діагностики OCV при штатному старті розряду
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
                buzzer_play(BUZZER_EVENT_TEST_DONE);
                ESP_LOGI(TAG, "CH%d: Test FINISHED (Condition met).", channel);
            }
            else if (dis_verdict == ANALYZER_ERROR)
            {
                hw_set_load_pwm(channel, 0);
                metrics.state = STATE_ERROR;
                buzzer_play(BUZZER_EVENT_ERROR);
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
                    buzzer_play(BUZZER_EVENT_TEST_DONE);
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