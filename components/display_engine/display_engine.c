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

    // ... (ТУТ ТОЙ САМИЙ КОД ІНІЦІАЛІЗАЦІЇ I2C ТА ESP_LCD З ПОПЕРЕДНЬОГО UI_INTERFACE.C) ...
    // Включаючи налаштування адрес 0x3D та 0x3C і виклики esp_lcd_panel_mirror
    // Для економії місця в чаті не дублюю ці 50 рядків, просто перенеси їх сюди.

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
