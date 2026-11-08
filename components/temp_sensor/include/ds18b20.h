#ifndef DS18B20_H
#define DS18B20_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"

// Команди DS18B20
#define DS18B20_CMD_MATCH_ROM 0x55
#define DS18B20_CMD_CONVERT_T 0x44
#define DS18B20_CMD_WRITE_SCRATCH 0x4E
#define DS18B20_CMD_READ_SCRATCH 0xBE
#define DS18B20_CMD_COPY_SCRATCH 0x48

// API для роботи з конкретним датчиком
bool ds18b20_request_temperature(gpio_num_t pin, const uint8_t *rom);
bool ds18b20_read_temperature(gpio_num_t pin, const uint8_t *rom, int32_t *temp_mc);
bool ds18b20_write_scratchpad(gpio_num_t pin, const uint8_t *rom, uint8_t th, uint8_t tl, uint8_t config);
bool ds18b20_copy_scratchpad(gpio_num_t pin, const uint8_t *rom);

#endif // DS18B20_H