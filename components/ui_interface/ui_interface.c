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
#include <string.h>

static const char *TAG = "UI";

static i2c_master_bus_handle_t s_oled_bus_handle = NULL;
static esp_lcd_panel_handle_t s_panel_left = NULL;
static esp_lcd_panel_handle_t s_panel_right = NULL;

// Незалежні фреймбуфери для кожного екрана 128x64 (1024 байти кожен)
static uint8_t fb_left[1024] = {0};
static uint8_t fb_right[1024] = {0};

static void ui_task(void *pvParameters)
{
    while (1)
    {
        // Поки що тестове оновлення кадрів
        vTaskDelay(pdMS_TO_TICKS(1000));
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

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &s_oled_bus_handle));

    // Налаштування лівого екрана (0x3C)
    esp_lcd_panel_io_handle_t io_left = NULL;
    esp_lcd_panel_io_i2c_config_t io_config_left = {
        .dev_addr = 0x3C,
        .scl_speed_hz = 400000,
        .control_phase_bytes = 1,
        .dc_bit_offset = 6,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(s_oled_bus_handle, &io_config_left, &io_left));

    // Налаштування правого екрана (0x3D)
    esp_lcd_panel_io_handle_t io_right = NULL;
    esp_lcd_panel_io_i2c_config_t io_config_right = {
        .dev_addr = 0x3D,
        .scl_speed_hz = 400000,
        .control_phase_bytes = 1,
        .dc_bit_offset = 6,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(s_oled_bus_handle, &io_config_right, &io_right));

    esp_lcd_panel_dev_config_t panel_config = {
        .bits_per_pixel = 1,
        .reset_gpio_num = -1,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io_left, &panel_config, &s_panel_left));
    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io_right, &panel_config, &s_panel_right));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel_left));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel_left));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel_left, true));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel_right));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel_right));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel_right, true));

    // Очищення пам'яті дисплеїв (стерти стартовий "шум")
    memset(fb_left, 0, sizeof(fb_left));
    memset(fb_right, 0, sizeof(fb_right));
    esp_lcd_panel_draw_bitmap(s_panel_left, 0, 0, 128, 64, fb_left);
    esp_lcd_panel_draw_bitmap(s_panel_right, 0, 0, 128, 64, fb_right);

    ESP_LOGI(TAG, "Dual independent OLED panels initialized successfully");

    xTaskCreate(ui_task, "ui_task", 4096, NULL, configMAX_PRIORITIES - 3, NULL);

    return ESP_OK;
}

void ui_update_displays(void)
{
    esp_lcd_panel_draw_bitmap(s_panel_left, 0, 0, 128, 64, fb_left);
    esp_lcd_panel_draw_bitmap(s_panel_right, 0, 0, 128, 64, fb_right);
}