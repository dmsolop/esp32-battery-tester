#include "display_engine.h"
#include "font.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include <string.h>

static const char *TAG = "DISP_ENG";

static i2c_master_bus_handle_t s_oled_bus_handle = NULL;
static esp_lcd_panel_handle_t s_panel_left = NULL;
static esp_lcd_panel_handle_t s_panel_right = NULL;

static uint8_t fb_left[1024] = {0};
static uint8_t fb_right[1024] = {0};

esp_err_t display_engine_init(void)
{
    ESP_LOGI(TAG, "Initializing Display Engine...");

    i2c_master_bus_config_t bus_config = {
        .i2c_port = -1,
        .sda_io_num = CONFIG_I2C_OLED_SDA_PIN,
        .scl_io_num = CONFIG_I2C_OLED_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &s_oled_bus_handle));

    // Налаштування лівого екрана (Двоколірний, 0x3D)
    esp_lcd_panel_io_handle_t io_left = NULL;
    esp_lcd_panel_io_i2c_config_t io_config_left = {
        .dev_addr = 0x3D,
        .scl_speed_hz = 400000,
        .control_phase_bytes = 1,
        .dc_bit_offset = 6,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(s_oled_bus_handle, &io_config_left, &io_left));

    // Налаштування правого екрана (Монохромний, 0x3C)
    esp_lcd_panel_io_handle_t io_right = NULL;
    esp_lcd_panel_io_i2c_config_t io_config_right = {
        .dev_addr = 0x3C,
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
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel_left, true, true));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel_right));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel_right));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel_right, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel_right, true, true));

    // Очищення пам'яті дисплеїв
    memset(fb_left, 0, sizeof(fb_left));
    memset(fb_right, 0, sizeof(fb_right));
    esp_lcd_panel_draw_bitmap(s_panel_left, 0, 0, 128, 64, fb_left);
    esp_lcd_panel_draw_bitmap(s_panel_right, 0, 0, 128, 64, fb_right);

    ESP_LOGI(TAG, "Display Engine initialized successfully");
    return ESP_OK;
}

void display_engine_update(void)
{
    esp_lcd_panel_draw_bitmap(s_panel_left, 0, 0, 128, 64, fb_left);
    esp_lcd_panel_draw_bitmap(s_panel_right, 0, 0, 128, 64, fb_right);
}

void display_engine_clear(void)
{
    memset(fb_left, 0, sizeof(fb_left));
    memset(fb_right, 0, sizeof(fb_right));
}

void display_engine_draw_pixel(int x, int y, uint8_t color)
{
    if (x < 0 || x >= 256 || y < 0 || y >= 64)
        return;

    uint8_t *fb = (x < 128) ? fb_left : fb_right;
    int local_x = (x < 128) ? x : x - 128;
    int index = local_x + ((y / 8) * 128);

    if (color)
    {
        fb[index] |= (1 << (y % 8));
    }
    else
    {
        fb[index] &= ~(1 << (y % 8));
    }
}

void display_engine_draw_char(int x, int y, char c, uint8_t color)
{
    if (c < 32 || c > 127)
        return;
    int font_idx = c - 32;

    for (int i = 0; i < 5; i++)
    {
        uint8_t line = font_5x7[font_idx][i];
        for (int j = 0; j < 7; j++)
        {
            if (line & (1 << j))
                display_engine_draw_pixel(x + i, y + j, color);
            else
                display_engine_draw_pixel(x + i, y + j, !color);
        }
    }
}

void display_engine_draw_string(int x, int y, const char *str, uint8_t color)
{
    while (*str)
    {
        display_engine_draw_char(x, y, *str, color);
        x += 6;
        str++;
    }
}

// Нова функція рендерингу іконок (до 8 пікселів у ширину)
void display_engine_draw_bitmap(int x, int y, const uint8_t *bitmap, int w, int h, uint8_t color)
{
    for (int j = 0; j < h; j++)
    {
        for (int i = 0; i < w; i++)
        {
            // Читаємо біти зліва направо (старший біт 7 - це лівий край пікселя)
            if (bitmap[j] & (1 << (7 - i)))
            {
                display_engine_draw_pixel(x + i, y + j, color);
            }
        }
    }
}
