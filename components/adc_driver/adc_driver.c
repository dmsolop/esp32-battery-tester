#include "adc_driver.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0
#define I2C_MASTER_TIMEOUT_MS 1000
#ifdef NDEBUG
#define I2C_MASTER_FREQ_HZ 400000 // Release: Fast mode для PCB
#else
#define I2C_MASTER_FREQ_HZ 100000 // Debug: Standard mode для макетної плати
#endif

static const char *TAG = "ADC_DRIVER";
static SemaphoreHandle_t s_i2c_mutex = NULL;

esp_err_t adc_driver_init(void)
{
    if (s_i2c_mutex != NULL)
    {
        return ESP_OK;
    }

    s_i2c_mutex = xSemaphoreCreateMutex();
    if (s_i2c_mutex == NULL)
    {
        ESP_LOGE(TAG, "Failed to create I2C mutex");
        return ESP_FAIL;
    }

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "I2C param config failed");
        return err;
    }

    err = i2c_driver_install(I2C_MASTER_NUM, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "I2C driver install failed");
        return err;
    }

    ESP_LOGI(TAG, "I2C initialized at %d Hz", I2C_MASTER_FREQ_HZ);
    return ESP_OK;
}

static esp_err_t i2c_read_adc(uint8_t channel, bool is_current, uint32_t *out_val)
{
    if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        // TODO: Тут буде реальний запис у Config Register ADS1115
        // та зчитування Conversion Register

        // Симуляція даних АЦП
        if (!is_current)
        {
            *out_val = 4100000 + (channel * 10000);
        }
        else
        {
            *out_val = 1500000 + (channel * 50000);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
        xSemaphoreGive(s_i2c_mutex);
        return ESP_OK;
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