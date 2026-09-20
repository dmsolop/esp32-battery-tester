#ifndef DS18B20_H
#define DS18B20_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"

// Команди DS18B20
#define DS18B20_CMD_MATCH_ROM 0x55
#define DS18B20_CMD_CONVERT_T 0x44
#define DS18B20_CMD_READ_SCRATCH 0xBE

// API для роботи з конкретним датчиком
bool ds18b20_request_temperature(gpio_num_t pin, const uint8_t *rom);
bool ds18b20_read_temperature(gpio_num_t pin, const uint8_t *rom, int32_t *temp_mc);

#endif // DS18B20_H