#ifndef ONEWIRE_H
#define ONEWIRE_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"

// Фізичний рівень
void onewire_init(gpio_num_t pin);
bool onewire_reset(gpio_num_t pin);
void onewire_write_bit(gpio_num_t pin, uint8_t bit);
uint8_t onewire_read_bit(gpio_num_t pin);
void onewire_write_byte(gpio_num_t pin, uint8_t data);
uint8_t onewire_read_byte(gpio_num_t pin);

// Рівень пошуку (Search ROM)
void onewire_reset_search(void);
bool onewire_search(gpio_num_t pin, uint8_t *newAddr);

#endif // ONEWIRE_H