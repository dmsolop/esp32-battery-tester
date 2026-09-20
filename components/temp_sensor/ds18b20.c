#include "ds18b20.h"
#include "onewire.h"
#include "esp_log.h"

// Внутрішня функція для вибору конкретного датчика на шині
static bool onewire_match_rom(gpio_num_t pin, const uint8_t *rom)
{
    if (!onewire_reset(pin))
    {
        return false; // Шина порожня або замикання
    }

    onewire_write_byte(pin, DS18B20_CMD_MATCH_ROM); // 0x55

    // Відправляємо 8 байт (64 біти) унікальної адреси
    for (int i = 0; i < 8; i++)
    {
        onewire_write_byte(pin, rom[i]);
    }
    return true;
}

bool ds18b20_request_temperature(gpio_num_t pin, const uint8_t *rom)
{
    if (!onewire_match_rom(pin, rom))
    {
        return false;
    }
    // Команда на початок конвертації температури
    onewire_write_byte(pin, DS18B20_CMD_CONVERT_T); // 0x44
    return true;
}

bool ds18b20_read_temperature(gpio_num_t pin, const uint8_t *rom, int32_t *temp_mc)
{
    if (!onewire_match_rom(pin, rom))
    {
        return false;
    }

    // Команда на зчитування оперативної пам'яті (Scratchpad) датчика
    onewire_write_byte(pin, DS18B20_CMD_READ_SCRATCH); // 0xBE

    // Зчитуємо перші два байти (LSB та MSB температури)
    uint8_t lsb = onewire_read_byte(pin);
    uint8_t msb = onewire_read_byte(pin);

    // За стандартом треба зчитати ще 7 байт для CRC,
    // але для базового зчитування перших двох достатньо.
    // Перериваємо передачу імпульсом скидання.
    onewire_reset(pin);

    // Об'єднуємо два байти у 16-бітне число зі знаком
    int16_t raw_temp = (int16_t)((msb << 8) | lsb);

    // Конвертація у міліградуси Цельсія (цілочисельна математика)
    // 1 крок АЦП DS18B20 = 0.0625 °C (або 62.5 mC)
    *temp_mc = (int32_t)raw_temp * 625 / 10;

    return true;
}