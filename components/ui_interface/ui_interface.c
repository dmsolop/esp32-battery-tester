#include "ui_interface.h"
#include "system_state.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "sdkconfig.h"

#ifndef CONFIG_MAX_CHANNELS
#define CONFIG_MAX_CHANNELS 4
#endif

static const char *TAG = "UI";

static i2c_master_bus_handle_t s_oled_bus_handle = NULL;
static esp_lcd_panel_handle_t s_panel_left = NULL;
static esp_lcd_panel_handle_t s_panel_right = NULL;

static void ui_task(void *pvParameters)
{
    channel_metrics_t metrics;

    while (1)
    {
        // Тут будемо малювати у локальний буфер 256x64
        // і відправляти його через esp_lcd_panel_draw_bitmap на лівий та правий екрани.
        // Поки що залишаємо логування метрик.

        for (int i = 0; i < CONFIG_MAX_CHANNELS; i++)
        {
            if (system_state_get_metrics(i, &metrics) == ESP_OK)
            {
                ESP_LOGI(TAG, "CH%d | State: %d | V: %lu uV | I: %lu uA",
                         i, metrics.state, metrics.voltage_uv, metrics.current_ua);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

esp_err_t ui_interface_init(void)
{
    ESP_LOGI(TAG, "Initializing Secondary I2C Bus for OLEDs...");

    i2c_master_bus_config_t bus_config = {
        .i2c_port = -1,
        .sda_io_num = CONFIG_I2C_OLED_SDA_PIN,
        .scl_io_num = CONFIG_I2C_OLED_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t err = i2c_new_master_bus(&bus_config, &s_oled_bus_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create OLED I2C bus");
        return err;
    }

    ESP_LOGI(TAG, "--- I2C Bus Scanner ---");
    uint8_t found_devices = 0;
    for (uint8_t addr = 1; addr < 127; addr++)
    {
        // Відправляємо пустий запит для перевірки наявності пристрою (таймаут 50 мс)
        if (i2c_master_probe(s_oled_bus_handle, addr, 50) == ESP_OK)
        {
            ESP_LOGI(TAG, "-> Found I2C device at address: 0x%02X", addr);
            found_devices++;
        }
    }
    ESP_LOGI(TAG, "-----------------------");

    if (found_devices < 2)
    {
        ESP_LOGW(TAG, "Found only %d device(s). Check your R3/R4 resistors and wiring!", found_devices);
        // Не блокуємо виконання, щоб можна було побачити лог
    }

    // Налаштування драйвера esp_lcd для Лівого екрана (зазвичай 0x3C)
    esp_lcd_panel_io_handle_t io_left = NULL;
    esp_lcd_panel_io_i2c_config_t io_config_left = {
        .dev_addr = 0x3C,
        .control_phase_bytes = 1,
        .dc_bit_offset = 6,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    esp_lcd_new_panel_io_i2c(s_oled_bus_handle, &io_config_left, &io_left);

    // Налаштування драйвера esp_lcd для Правого екрана (зазвичай 0x3D)
    esp_lcd_panel_io_handle_t io_right = NULL;
    esp_lcd_panel_io_i2c_config_t io_config_right = {
        .dev_addr = 0x3D,
        .control_phase_bytes = 1,
        .dc_bit_offset = 6,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    esp_lcd_new_panel_io_i2c(s_oled_bus_handle, &io_config_right, &io_right);

    esp_lcd_panel_dev_config_t panel_config = {
        .bits_per_pixel = 1,
        .reset_gpio_num = -1, // У нас немає апаратного піна Reset для I2C екранів
    };

    // Ініціалізуємо обидві панелі як SSD1306
    esp_lcd_new_panel_ssd1306(io_left, &panel_config, &s_panel_left);
    esp_lcd_new_panel_ssd1306(io_right, &panel_config, &s_panel_right);

    esp_lcd_panel_reset(s_panel_left);
    esp_lcd_panel_init(s_panel_left);
    esp_lcd_panel_disp_on_off(s_panel_left, true);

    esp_lcd_panel_reset(s_panel_right);
    esp_lcd_panel_init(s_panel_right);
    esp_lcd_panel_disp_on_off(s_panel_right, true);

    ESP_LOGI(TAG, "Dual OLED Panels initialized");

    xTaskCreate(ui_task, "ui_task", 4096, NULL, configMAX_PRIORITIES - 3, NULL);

    return ESP_OK;
}