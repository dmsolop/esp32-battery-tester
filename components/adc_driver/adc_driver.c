#include "adc_driver.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"  // Макроси Kconfig
#include "pwm_driver.h" // Для апаратного байпасу ШІМ

#define I2C_MASTER_FREQ_HZ 100000
#define I2C_MASTER_PORT_NUM -1
#define I2C_MASTER_TIMEOUT_MS 1000

#define ADS1115_I2C_ADDRESS 0x48
#define ADS1115_REG_CONVERSION 0x00
#define ADS1115_REG_CONFIG 0x01

static uint32_t s_mock_voltage[4] = {4100000, 4110000, 4120000, 4130000};
static uint32_t s_mock_current[4] = {1450000, 1500000, 1550000, 1600000};

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
    return ESP_OK;
}

static esp_err_t i2c_read_adc(uint8_t channel, bool is_current, uint32_t *out_val)
{
    if (channel > 3)
        return ESP_ERR_INVALID_ARG;

    if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        esp_err_t err = ESP_OK;

#ifndef NDEBUG
        if (!is_current)
        {
            *out_val = s_mock_voltage[channel];
        }
        else
        {
            *out_val = s_mock_current[channel];
        }
        vTaskDelay(pdMS_TO_TICKS(10));
#else
        if (s_ads_handles[channel] != NULL)
        {
            uint16_t config = 0x0383;
            config |= (is_current ? 0xD000 : 0xC000);

            uint8_t write_buf[3] = {
                ADS1115_REG_CONFIG,
                (uint8_t)(config >> 8),
                (uint8_t)(config & 0xFF)};

            err = i2c_master_transmit(s_ads_handles[channel], write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS);
            if (err == ESP_OK)
            {
                vTaskDelay(pdMS_TO_TICKS(10));

                uint8_t reg_addr = ADS1115_REG_CONVERSION;
                uint8_t read_buf[2] = {0};

                err = i2c_master_transmit_receive(s_ads_handles[channel], &reg_addr, 1, read_buf, 2, I2C_MASTER_TIMEOUT_MS);
                if (err == ESP_OK)
                {
                    int16_t raw_adc = (read_buf[0] << 8) | read_buf[1];
                    if (raw_adc < 0)
                        raw_adc = 0;
                    *out_val = (uint32_t)raw_adc * 125;
                }
            }
        }
        else
        {
            err = ESP_ERR_INVALID_STATE;
        }
#endif

        xSemaphoreGive(s_i2c_mutex);
        return err;
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t adc_driver_read_voltage(uint8_t channel, uint32_t *voltage_uv)
{
    return i2c_read_adc(channel, false, voltage_uv);
}

esp_err_t adc_driver_read_current(uint8_t channel, uint32_t *current_ua)
{
    return i2c_read_adc(channel, true, current_ua);
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