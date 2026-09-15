#include "adc_driver.h"
#include "driver/i2c_master.h" // Використовуємо новий драйвер
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef NDEBUG
#define I2C_MASTER_FREQ_HZ 400000 // Release: Fast mode для PCB (використаємо при додаванні пристрою)
#else
#define I2C_MASTER_FREQ_HZ 100000 // Debug: Standard mode для макетної плати (використаємо при додаванні пристрою)
#endif
#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_PORT_NUM -1 // -1 дозволяє драйверу автоматично обрати вільний порт (I2C_NUM_0 або I2C_NUM_1)
#define I2C_MASTER_TIMEOUT_MS 1000

// Стандартна адреса ADS1115 (ADDR -> GND)
#define ADS1115_I2C_ADDRESS 0x48
#define ADS1115_REG_CONVERSION 0x00
#define ADS1115_REG_CONFIG 0x01

static uint32_t s_mock_voltage[4] = {4100000, 4110000, 4120000, 4130000};
static uint32_t s_mock_current[4] = {1450000, 1500000, 1550000, 1600000}; // 1.5A для старту

static const char *TAG = "ADC_DRIVER";
static SemaphoreHandle_t s_i2c_mutex = NULL;

static i2c_master_bus_handle_t s_bus_handle = NULL; // Зберігаємо хендл шини для подальшого додавання пристроїв
static i2c_master_dev_handle_t s_ads_handle = NULL; // Хендл нашого АЦП

esp_err_t adc_driver_init(void)
{
    if (s_bus_handle != NULL && s_i2c_mutex != NULL)
    {
        return ESP_OK; // Вже ініціалізовано
    }

    s_i2c_mutex = xSemaphoreCreateMutex();
    if (s_i2c_mutex == NULL)
    {
        ESP_LOGE(TAG, "Failed to create I2C mutex");
        return ESP_FAIL;
    }

    // Налаштування самої шини (тільки піни та підтяжки)
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_PORT_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    // Створюємо нову шину
    esp_err_t err = i2c_new_master_bus(&bus_config, &s_bus_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "New I2C master bus initialization failed");
        return err;
    }

    // Реєстрація пристрою ADS1115 на створеній шині
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ADS1115_I2C_ADDRESS,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    err = i2c_master_bus_add_device(s_bus_handle, &dev_cfg, &s_ads_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to add ADS1115 to I2C bus");
        return err;
    }

    ESP_LOGI(TAG, "I2C bus initialized. ADS1115 registered at 0x%02X (%d Hz)", ADS1115_I2C_ADDRESS, I2C_MASTER_FREQ_HZ);
    return ESP_OK;
}

static esp_err_t i2c_read_adc(uint8_t channel, bool is_current, uint32_t *out_val)
{
    if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        esp_err_t err = ESP_OK;

        // Фізичне "залізо" працює тільки для Каналу 0
        if (channel == 0)
        {
            // Налаштування Config Register (16 біт)
            // База: Single-shot (Bit15=1), PGA +/- 4.096V (Bits11-9=001), 128 SPS (Bits7-5=100)
            // MUX (Bits14-12): 100 для AIN0 (напруга), 101 для AIN1 (струм)
            uint16_t config = 0x0383;                 // Базові біти (0000 0011 1000 0011)
            config |= (is_current ? 0xD000 : 0xC000); // Додаємо MUX та біт старту (OS)

            // I2C передає старший байт першим (MSB first)
            uint8_t write_buf[3] = {
                ADS1115_REG_CONFIG,
                (uint8_t)(config >> 8),
                (uint8_t)(config & 0xFF)};

            // Запис конфігурації
            err = i2c_master_transmit(s_ads_handle, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS);
            if (err == ESP_OK)
            {
                // Чекаємо завершення перетворення. Для 128 SPS це ~8 мс. Даємо 10 мс.
                vTaskDelay(pdMS_TO_TICKS(10));

                uint8_t reg_addr = ADS1115_REG_CONVERSION;
                uint8_t read_buf[2] = {0};

                // Читаємо 2 байти результату. Функція сама відправляє адресу регістра, робить Repeated Start і читає.
                err = i2c_master_transmit_receive(s_ads_handle, &reg_addr, 1, read_buf, 2, I2C_MASTER_TIMEOUT_MS);
                if (err == ESP_OK)
                {
                    // Склеюємо два байти у знакове 16-бітне число
                    int16_t raw_adc = (read_buf[0] << 8) | read_buf[1];

                    if (raw_adc < 0)
                        raw_adc = 0; // Відкидаємо можливий шум нижче нуля

                    // При PGA +/- 4.096V, 1 біт = 125 мікровольт (uV)
                    // Поки що повертаємо мікровольти і для струму, і для напруги (додамо шунт пізніше)
                    *out_val = (uint32_t)raw_adc * 125;
                }
            }
        }
        else
        {
            // Заглушки для каналів 1, 2, 3
            if (!is_current)
            {
                *out_val = s_mock_voltage[channel];
            }
            else
            {
                *out_val = s_mock_current[channel];
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }

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