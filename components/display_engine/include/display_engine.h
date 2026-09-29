#pragma once
#include "esp_err.h"
#include <stdint.h>

esp_err_t display_engine_init(void);
void display_engine_update(void);
void display_engine_clear(void);

// Графічні примітиви
void display_engine_draw_pixel(int x, int y, uint8_t color);
void display_engine_draw_char(int x, int y, char c, uint8_t color);
void display_engine_draw_string(int x, int y, const char *str, uint8_t color);
void display_engine_draw_bitmap(int x, int y, const uint8_t *bitmap, int w, int h, uint8_t color);
