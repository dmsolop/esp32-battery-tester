#include "adc_driver.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"  // Макроси Kconfig
#include "pwm_driver.h" // Для апаратного байпасу ШІМ
#include "nvs_flash.h"
#include "nvs.h"

#define I2C_MASTER_FREQ_HZ 100000
#define I2C_MASTER_PORT_NUM -1
#define I2C_MASTER_TIMEOUT_MS 1000

#define ADS1115_I2C_ADDRESS 0x48
#define ADS1115_REG_CONVERSION 0x00
#define ADS1115_REG_CONFIG 0x01

// ADS1115 Config bits (PGA +/-0.256V, 128 SPS, single-shot mode)
// AIN0 vs GND: MUX = 100 -> 0xCB83 (I_SENSE_Q1)
// AIN1 vs GND: MUX = 101 -> 0xDB83 (I_SENSE_Q2)
// AIN2 vs AIN3 (диференційно): MUX = 011, PGA +/-4.096V -> 0xB383 (V_SENSE Kelvin)
#define ADS1115_CONFIG_CURRENT_AIN0 0xCB83
#define ADS1115_CONFIG_CURRENT_AIN1 0xDB83
#define ADS1115_CONFIG_VOLTAGE_DIFF 0xB383

#ifndef CONFIG_SHUNT_RESISTOR_MOHM
#define CONFIG_SHUNT_RESISTOR_MOHM 270 // За замовчуванням 270 мОм (макет), для фінальної плати 5 мОм
#endif

typedef struct
{
    int32_t voltage_offset_uv;
    int32_t current_offset_q1_ua;
    int32_t current_offset_q2_ua;
    bool dual_branch_enabled;
} adc_calib_t;

static adc_calib_t s_calib[4] = {0};

static uint32_t s_mock_voltage[4] = {4100000, 4110000, 4120000, 4130000};
static int32_t s_mock_current[4] = {1450000, 1500000, 1550000, 1600000};

static const char *TAG = "ADC_DRIVER";
static SemaphoreHandle_t s_i2c_mutex = NULL;
static i2c_master_bus_handle_t s_bus_handle = NULL;
static i2c_master_dev_handle_t s_ads_handles[4] = {NULL};
static const uint8_t s_ads_addrs[4] = {0x48, 0x49, 0x4A, 0x4B};

// Хендл для передачі нотифікації
static TaskHandle_t s_safety_task_handle = NULL;

// Обробник переривання для піна ALERT
static void IRAM_ATTR adc_alert_isr_handler(void *arg)
{
    // 1. Апаратний байпас: миттєво відключаємо силове навантаження
    pwm_driver_emergency_stop_isr();

    // 2. Будимо Task_Safety для програмної обробки помилки
    if (s_safety_task_handle != NULL)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(s_safety_task_handle, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken)
        {
            portYIELD_FROM_ISR(); // Негайне перемикання контексту
        }
    }
}

void adc_driver_register_safety_task(TaskHandle_t task)
{
    s_safety_task_handle = task;
}

esp_err_t adc_driver_init(void)
{
    if (s_bus_handle != NULL && s_i2c_mutex != NULL)
    {
        return ESP_OK;
    }

    s_i2c_mutex = xSemaphoreCreateMutex();
    if (s_i2c_mutex == NULL)
    {
        ESP_LOGE(TAG, "Failed to create I2C mutex");
        return ESP_FAIL;
    }

    // Налаштування I2C на базі макросів Kconfig
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_PORT_NUM,
        .sda_io_num = CONFIG_I2C_SDA_PIN,
        .scl_io_num = CONFIG_I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t err = i2c_new_master_bus(&bus_config, &s_bus_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "New I2C master bus initialization failed");
        return err;
    }

    for (int i = 0; i < 4; i++)
    {
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = s_ads_addrs[i],
            .scl_speed_hz = I2C_MASTER_FREQ_HZ,
        };
        err = i2c_master_bus_add_device(s_bus_handle, &dev_cfg, &s_ads_handles[i]);
        if (err != ESP_OK)
        {
            ESP_LOGE("ADC_DRV", "Failed to add ADS1115 for CH%d at 0x%02X", i, s_ads_addrs[i]);
            return err;
        }
    }

    // Налаштування апаратного переривання для піна ALERT
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE, // Переривання по спаду напруги
        .pin_bit_mask = (1ULL << CONFIG_ADC_ALERT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, // Підтяжка для відкритого стоку (Wired-OR)
        .pull_down_en = GPIO_PULLDOWN_DISABLE};
    gpio_config(&io_conf);

    // Встановлення служби переривань (прапорець 0 = за замовчуванням)
    gpio_install_isr_service(0);
    gpio_isr_handler_add(CONFIG_ADC_ALERT_PIN, adc_alert_isr_handler, NULL);

    ESP_LOGI(TAG, "I2C bus initialized (SDA:%d, SCL:%d). ALERT int configured on pin %d",
             CONFIG_I2C_SDA_PIN, CONFIG_I2C_SCL_PIN, CONFIG_ADC_ALERT_PIN);

    // Автоматичне відновлення калибровок з NVS
    nvs_handle_t nvs_handle;
    if (nvs_open("storage", NVS_READONLY, &nvs_handle) == ESP_OK)
    {
        for (int ch = 0; ch < 4; ch++)
        {
            char key_v[16], key_i[16];
            snprintf(key_v, sizeof(key_v), "ch%d_v_off_mv", ch);
            snprintf(key_i, sizeof(key_i), "ch%d_zero_i", ch);

            int32_t v_off_mv = 0;
            int32_t i_off_ua = 0;
            if (nvs_get_i32(nvs_handle, key_v, &v_off_mv) == ESP_OK)
            {
                s_calib[ch].voltage_offset_uv = v_off_mv * 1000;
            }
            if (nvs_get_i32(nvs_handle, key_i, &i_off_ua) == ESP_OK)
            {
                s_calib[ch].current_offset_q1_ua = i_off_ua;
                s_calib[ch].current_offset_q2_ua = 0;
            }
            ESP_LOGI(TAG, "CH%d: Loaded NVS calib: V_off=%ld mV, I_off=%ld uA",
                     ch, (long)(s_calib[ch].voltage_offset_uv / 1000), (long)s_calib[ch].current_offset_q1_ua);
        }
        nvs_close(nvs_handle);
    }

    return ESP_OK;
}

void adc_driver_set_dual_branch_mode(uint8_t channel, bool enable_dual)
{
    if (channel < 4)
    {
        s_calib[channel].dual_branch_enabled = enable_dual;
        ESP_LOGI(TAG, "CH%d: Dual branch mode set to %s", channel, enable_dual ? "ENABLED" : "DISABLED");
    }
}

void adc_driver_set_zero_offset(uint8_t channel, int32_t v_off_uv, int32_t i_off_ua)
{
    if (channel < 4)
    {
        s_calib[channel].voltage_offset_uv = v_off_uv;
        // Для сумісності з поточним макетом: якщо режим одноплечовий, калібруємо Q1 і скидаємо Q2
        s_calib[channel].current_offset_q1_ua = i_off_ua;
        s_calib[channel].current_offset_q2_ua = 0;

        // Збереження в NVS
        nvs_handle_t nvs_handle;
        if (nvs_open("storage", NVS_READWRITE, &nvs_handle) == ESP_OK)
        {
            char key_v[16], key_i[16];
            snprintf(key_v, sizeof(key_v), "ch%d_v_off_mv", channel);
            snprintf(key_i, sizeof(key_i), "ch%d_zero_i", channel);

            nvs_set_i32(nvs_handle, key_v, v_off_uv / 1000);
            nvs_set_i32(nvs_handle, key_i, i_off_ua);
            nvs_commit(nvs_handle);
            nvs_close(nvs_handle);
        }
    }
}

int32_t adc_driver_get_zero_offset_voltage_mv(uint8_t channel)
{
    if (channel < 4)
    {
        return s_calib[channel].voltage_offset_uv / 1000;
    }
    return 0;
}

void adc_driver_set_voltage_offset_mv(uint8_t channel, int32_t v_off_mv)
{
    if (channel < 4)
    {
        s_calib[channel].voltage_offset_uv = v_off_mv * 1000;

        nvs_handle_t nvs_handle;
        if (nvs_open("storage", NVS_READWRITE, &nvs_handle) == ESP_OK)
        {
            char key_v[16];
            snprintf(key_v, sizeof(key_v), "ch%d_v_off_mv", channel);
            nvs_set_i32(nvs_handle, key_v, v_off_mv);
            nvs_commit(nvs_handle);
            nvs_close(nvs_handle);
        }
    }
}

int32_t adc_driver_get_zero_offset_current(uint8_t channel)
{
    if (channel < 4)
    {
        return s_calib[channel].current_offset_q1_ua;
    }
    return 0;
}

void adc_driver_set_zero_offset_branch(uint8_t channel, adc_current_channel_t branch, int32_t i_off_ua)
{
    if (channel < 4)
    {
        if (branch == ADC_CURRENT_SENSE_Q1)
        {
            s_calib[channel].current_offset_q1_ua = i_off_ua;
        }
        else if (branch == ADC_CURRENT_SENSE_Q2)
        {
            s_calib[channel].current_offset_q2_ua = i_off_ua;
        }
    }
}

static esp_err_t ads1115_read_raw(uint8_t channel, uint16_t config, int16_t *out_raw)
{
    if (channel > 3 || out_raw == NULL)
        return ESP_ERR_INVALID_ARG;

    if (s_ads_handles[channel] == NULL)
        return ESP_ERR_INVALID_STATE;

    if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        uint8_t write_buf[3] = {
            ADS1115_REG_CONFIG,
            (uint8_t)(config >> 8),
            (uint8_t)(config & 0xFF)};

        esp_err_t err = i2c_master_transmit(s_ads_handles[channel], write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS);
        if (err == ESP_OK)
        {
            // 128 SPS -> ~7.81 ms період перетворення ADS1115. Затримка 10 мс гарантує готовність результату
            vTaskDelay(pdMS_TO_TICKS(10));

            uint8_t reg_addr = ADS1115_REG_CONVERSION;
            uint8_t read_buf[2] = {0};

            err = i2c_master_transmit_receive(s_ads_handles[channel], &reg_addr, 1, read_buf, 2, I2C_MASTER_TIMEOUT_MS);
            if (err == ESP_OK)
            {
                *out_raw = (int16_t)((read_buf[0] << 8) | read_buf[1]);
            }
        }

        xSemaphoreGive(s_i2c_mutex);
        return err;
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t adc_driver_read_voltage(uint8_t channel, uint32_t *voltage_uv)
{
    if (channel > 3 || voltage_uv == NULL)
        return ESP_ERR_INVALID_ARG;

    int16_t raw_adc = 0;
    esp_err_t err = ads1115_read_raw(channel, ADS1115_CONFIG_VOLTAGE_DIFF, &raw_adc);
    if (err == ESP_OK)
    {
        if (raw_adc < 0)
            raw_adc = 0;

        // 1 LSB при PGA +/-4.096V = 125 мкВ
        // Кельвін-дільник 21.5 (205k / 10k -> * 43 / 2)
        // Обчислення в uint64_t для виключення переповнення
        uint64_t adc_uv = (uint64_t)raw_adc * 125ULL;
        uint32_t raw_voltage = (uint32_t)((adc_uv * 43ULL) / 2ULL);

        int32_t compensated = (int32_t)raw_voltage - s_calib[channel].voltage_offset_uv;
        *voltage_uv = (compensated < 0) ? 0 : (uint32_t)compensated;
    }
    return err;
}

// Точний розрахунок струму з raw ADC без проміжного урізання мікровольт
// LSB при PGA +/-0.256V = 7.8125 мкВ = 125 / 16 мкВ
// I_uA = (V_shunt_uV * 1000) / R_shunt_mohm = (raw_adc * 125 * 1000) / (16 * R_shunt_mohm)
//      = (raw_adc * 15625ULL) / (2ULL * CONFIG_SHUNT_RESISTOR_MOHM)
static inline int32_t raw_adc_to_current_ua(int16_t raw_adc)
{
    if (raw_adc >= 0)
    {
        return (int32_t)(((uint64_t)raw_adc * 15625ULL) / (2ULL * CONFIG_SHUNT_RESISTOR_MOHM));
    }
    else
    {
        return -(int32_t)(((uint64_t)(-raw_adc) * 15625ULL) / (2ULL * CONFIG_SHUNT_RESISTOR_MOHM));
    }
}

esp_err_t adc_driver_read_current_branch(uint8_t channel, adc_current_channel_t branch, uint32_t *current_ua, int32_t *pid_current_ua)
{
    if (channel > 3 || current_ua == NULL || pid_current_ua == NULL)
        return ESP_ERR_INVALID_ARG;

    uint16_t config = (branch == ADC_CURRENT_SENSE_Q1) ? ADS1115_CONFIG_CURRENT_AIN0 : ADS1115_CONFIG_CURRENT_AIN1;
    int16_t raw_adc = 0;
    esp_err_t err = ads1115_read_raw(channel, config, &raw_adc);
    if (err == ESP_OK)
    {
        int32_t offset = (branch == ADC_CURRENT_SENSE_Q1) ? s_calib[channel].current_offset_q1_ua : s_calib[channel].current_offset_q2_ua;

        // ПІД-регулятор повинен бачити сирі від'ємні коливання шуму навколо нуля
        int32_t raw_current = raw_adc_to_current_ua(raw_adc);
        int32_t compensated = raw_current - offset;
        *pid_current_ua = compensated;
        *current_ua = (compensated < 0) ? 0 : (uint32_t)compensated;
    }
    return err;
}

esp_err_t adc_driver_read_current_dual(uint8_t channel, uint32_t *i_q1_ua, uint32_t *i_q2_ua, uint32_t *total_ua, int32_t *pid_total_ua)
{
    if (channel > 3 || i_q1_ua == NULL || i_q2_ua == NULL || total_ua == NULL || pid_total_ua == NULL)
        return ESP_ERR_INVALID_ARG;

    int32_t pid_q1 = 0;
    esp_err_t err = adc_driver_read_current_branch(channel, ADC_CURRENT_SENSE_Q1, i_q1_ua, &pid_q1);
    if (err != ESP_OK)
        return err;

    int32_t pid_q2 = 0;
    err = adc_driver_read_current_branch(channel, ADC_CURRENT_SENSE_Q2, i_q2_ua, &pid_q2);
    if (err != ESP_OK)
        return err;

    *pid_total_ua = pid_q1 + pid_q2;
    int32_t compensated_total = *pid_total_ua;
    *total_ua = (compensated_total < 0) ? 0 : (uint32_t)compensated_total;

    return ESP_OK;
}

esp_err_t adc_driver_read_current(uint8_t channel, uint32_t *current_ua, int32_t *pid_current_ua)
{
    if (channel > 3 || current_ua == NULL || pid_current_ua == NULL)
        return ESP_ERR_INVALID_ARG;

    if (s_calib[channel].dual_branch_enabled)
    {
        uint32_t i_q1 = 0, i_q2 = 0;
        return adc_driver_read_current_dual(channel, &i_q1, &i_q2, current_ua, pid_current_ua);
    }
    else
    {
        // Одноплечовий режим (поточний макет на 1 транзисторі / шунті 0.27 Ом):
        // Опитуємо AIN0 (I_SENSE_Q1)
        return adc_driver_read_current_branch(channel, ADC_CURRENT_SENSE_Q1, current_ua, pid_current_ua);
    }
}

void adc_driver_set_mock_voltage(uint8_t channel, uint32_t voltage_uv)
{
    if (channel < 4)
        s_mock_voltage[channel] = voltage_uv;
}

void adc_driver_set_mock_current(uint8_t channel, uint32_t current_ua)
{
    if (channel < 4)
        s_mock_current[channel] = current_ua;
}