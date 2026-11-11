#include "pwm_driver.h"
#include "driver/ledc.h"
#include "esp_attr.h"        // Для макросу IRAM_ATTR
#include "soc/ledc_struct.h" // Для прямого доступу до апаратних регістрів LEDC

#define PWM_TIMER LEDC_TIMER_0
#define FAN_PWM_TIMER LEDC_TIMER_1
#define PWM_MODE LEDC_LOW_SPEED_MODE
#define PWM_RESOLUTION LEDC_TIMER_12_BIT
#define PWM_FREQ_HZ 19531 // ~20 кГц (19531 Гц) для LEDC_TIMER_12_BIT (80MHz / 4096)

esp_err_t load_control_pwm_init(uint8_t channel, int gpio_num)
{
    // Ініціалізація загального таймера викликається лише один раз (для нульового каналу)
    if (channel == 0)
    {
        ledc_timer_config_t timer_conf = {
            .speed_mode = PWM_MODE,
            .timer_num = PWM_TIMER,
            .duty_resolution = PWM_RESOLUTION,
            .freq_hz = PWM_FREQ_HZ,
            .clk_cfg = LEDC_AUTO_CLK};
        esp_err_t err = ledc_timer_config(&timer_conf);
        if (err != ESP_OK)
            return err;
    }

    // Налаштування індивідуального каналу керування
    ledc_channel_config_t channel_conf = {
        .gpio_num = gpio_num,
        .speed_mode = PWM_MODE,
        .channel = (ledc_channel_t)channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = PWM_TIMER,
        .duty = 0, // Стартуємо з повністю закритого транзистора
        .hpoint = 0};

    return ledc_channel_config(&channel_conf);
}

esp_err_t load_control_pwm_set(uint8_t channel, uint32_t duty)
{
    // Захист від переповнення розрядності
    if (duty > PWM_MAX_DUTY)
    {
        duty = PWM_MAX_DUTY;
    }

    ledc_set_duty(PWM_MODE, (ledc_channel_t)channel, duty);
    return ledc_update_duty(PWM_MODE, (ledc_channel_t)channel);
}

esp_err_t load_control_fan_pwm_init(uint8_t channel, int gpio_num)
{
    if (channel >= 4 || gpio_num < 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Ініціалізація виділеного таймера кулерів FAN_PWM_TIMER (LEDC_TIMER_1, 30 Гц, 12 біт)
    // викликається лише один раз (для нульового каналу кулера)
    if (channel == 0)
    {
        ledc_timer_config_t fan_timer_conf = {
            .speed_mode = PWM_MODE,
            .timer_num = FAN_PWM_TIMER,
            .duty_resolution = PWM_RESOLUTION,
            .freq_hz = FAN_PWM_FREQ_HZ,
            .clk_cfg = LEDC_AUTO_CLK};
        esp_err_t err = ledc_timer_config(&fan_timer_conf);
        if (err != ESP_OK)
        {
            return err;
        }
    }

    // Канали кулерів призначаються на LEDC_CHANNEL_4..LEDC_CHANNEL_7
    ledc_channel_t fan_channel = (ledc_channel_t)(LEDC_CHANNEL_4 + channel);

    ledc_channel_config_t fan_conf = {
        .gpio_num = gpio_num,
        .speed_mode = PWM_MODE,
        .channel = fan_channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = FAN_PWM_TIMER,
        .duty = 0, // Стартуємо з вимкненого кулера
        .hpoint = 0};

    return ledc_channel_config(&fan_conf);
}

esp_err_t load_control_fan_pwm_set(uint8_t channel, uint32_t duty)
{
    if (channel >= 4)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Для 12-бітного таймера LEDC: при 100% передаємо 4096 (1 << LEDC_TIMER_12_BIT),
    // щоб апаратний ШІМ видавав постійний рівень HIGH (3.3V) без мікропровалу в кінці періоду.
    if (duty >= PWM_MAX_DUTY)
    {
        duty = (1 << PWM_RESOLUTION); // 4096
    }

    ledc_channel_t fan_channel = (ledc_channel_t)(LEDC_CHANNEL_4 + channel);
    ledc_set_duty(PWM_MODE, fan_channel, duty);
    return ledc_update_duty(PWM_MODE, fan_channel);
}

// Функція екстреної зупинки, що виконується з переривання (ISR)
void IRAM_ATTR pwm_driver_emergency_stop_isr(void)
{
    // Проходимо по 4 робочих каналах навантаження і на апаратному рівні вписуємо нуль у регістри
    for (int i = 0; i < 4; i++)
    {
        LEDC.channel_group[PWM_MODE].channel[i].duty.duty = 0;
        LEDC.channel_group[PWM_MODE].channel[i].conf1.duty_start = 1; // Тригер оновлення регістра
    }
    // Кулери при екстреній аварії вмикаємо на чисті 100% (4096) для посиленого охолодження аварійного транзистора
    for (int i = 4; i < 8; i++)
    {
        LEDC.channel_group[PWM_MODE].channel[i].duty.duty = ((1 << PWM_RESOLUTION) << 4);
        LEDC.channel_group[PWM_MODE].channel[i].conf1.duty_start = 1;
    }
}