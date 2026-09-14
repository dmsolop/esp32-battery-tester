#include "adc_driver.h"
#include "driver/i2c_master.h" // Використовуємо новий драйвер
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_PORT_NUM -1 // -1 дозволяє драйверу автоматично обрати вільний порт (I2C_NUM_0 або I2C_NUM_1)

#ifdef NDEBUG
#define I2C_MASTER_FREQ_HZ 400000 // Release: Fast mode для PCB (використаємо при додаванні пристрою)
#else
#define I2C_MASTER_FREQ_HZ 100000 // Debug: Standard mode для макетної плати (використаємо при додаванні пристрою)
#endif

#define I2C_MASTER_TIMEOUT_MS 1000
// Стандартна адреса ADS1115 (ADDR -> GND)
#define ADS1115_I2C_ADDRESS 0x48

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