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
#include "font.h"

static const char *TAG = "UI";

static i2c_master_bus_handle_t s_oled_bus_handle = NULL;
static esp_lcd_panel_handle_t s_panel_left = NULL;
static esp_lcd_panel_handle_t s_panel_right = NULL;

// Незалежні фреймбуфери для кожного екрана 128x64 (1024 байти кожен)
static uint8_t fb_left[1024] = {0};
static uint8_t fb_right[1024] = {0};

// Малювання одного пікселя на віртуальній сітці 256x64
static void draw_pixel(int x, int y, uint8_t color)
{
    // Відкидаємо координати поза межами екранів
    if (x < 0 || x >= 256 || y < 0 || y >= 64)
    {
        return;
    }

    // Маршрутизація: визначаємо, який буфер використовувати
    uint8_t *fb;
    int local_x;

    if (x < 128)
    {
        fb = fb_left;
        local_x = x;
    }
    else
    {
        fb = fb_right;
        local_x = x - 128;
    }

    // Розрахунок індексу байта (сторінкова адресація)
    int page = y / 8;
    int bit_pos = y % 8;
    int index = local_x + (page * 128);

    // Маніпуляція бітом
    if (color)
    {
        fb[index] |= (1 << bit_pos); // Засвітити (1)
    }
    else
    {
        fb[index] &= ~(1 << bit_pos); // Погасити (0)
    }
}

// Рендеринг одного символу (шрифт 5x7)
static void draw_char(int x, int y, char c, uint8_t color)
{
    // Відкидаємо недруковані символи
    if (c < 32 || c > 127)
        return;

    // Отримуємо індекс масиву (перший символ у нас пробіл, ASCII 32)
    int font_idx = c - 32;

    for (int i = 0; i < 5; i++)
    { // 5 вертикальних стовпців
        uint8_t line = font_5x7[font_idx][i];
        for (int j = 0; j < 7; j++)
        { // 7 пікселів у стовпці
            if (line & (1 << j))
            {
                draw_pixel(x + i, y + j, color);
            }
            else
            {
                draw_pixel(x + i, y + j, !color); // Затирання фону
            }
        }
    }
}

// Рендеринг рядка з урахуванням відступів
static void draw_string(int x, int y, const char *str, uint8_t color)
{
    while (*str)
    {
        draw_char(x, y, *str, color);
        x += 6; // Ширина символу (5) + проміжок (1 піксель)
        str++;
    }
}

static void ui_task(void *pvParameters)
{
    // Очищуємо буфери
    memset(fb_left, 0, sizeof(fb_left));
    memset(fb_right, 0, sizeof(fb_right));

    // Малюємо тестовий текст
    draw_string(0, 0, "TESTER DASHBOARD", 1);  // Лівий екран, верхній рядок (жовта зона)
    draw_string(0, 20, "CH1: 3.75V 1.00A", 1); // Лівий екран, нижче

    draw_string(128, 0, "SYSTEM METRICS", 1); // Правий екран (x = 128)
    draw_string(128, 20, "TEMP: 45C", 1);     // Правий екран

    // Відправляємо на дисплеї
    ui_update_displays();

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
        .dev_addr = 0x3D,
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

    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel_right));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel_right));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel_right, true));

    // Апаратний поворот зображення на 180 градусів
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel_left, true, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel_right, true, true));

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