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

// Локальна функція розрахунку Dallas CRC8
static uint8_t ds18b20_crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++)
    {
        uint8_t inbyte = data[i];
        for (uint8_t j = 0; j < 8; j++)
        {
            uint8_t mix = (crc ^ inbyte) & 0x01;
            crc >>= 1;
            if (mix)
                crc ^= 0x8C;
            inbyte >>= 1;
        }
    }
    return crc;
}

bool ds18b20_read_temperature(gpio_num_t pin, const uint8_t *rom, int32_t *temp_mc)
{
    if (!onewire_match_rom(pin, rom))
    {
        return false;
    }

    // Команда на зчитування оперативної пам'яті (Scratchpad) датчика
    onewire_write_byte(pin, DS18B20_CMD_READ_SCRATCH); // 0xBE

    // Зчитуємо всі 9 байтів
    uint8_t scratchpad[9];
    for (int i = 0; i < 9; i++)
    {
        scratchpad[i] = onewire_read_byte(pin);
    }

    // Перевірка контрольної суми (перші 8 байтів мають дати CRC, що лежить у 9-му)
    if (ds18b20_crc8(scratchpad, 8) != scratchpad[8])
    {
        ESP_LOGE("DS18B20", "CRC Error for ROM: %02X%02X...", rom[0], rom[1]);
        return false;
    }

    // Об'єднуємо два байти у 16-бітне число зі знаком (LSB - 0 байт, MSB - 1 байт)
    int16_t raw_temp = (int16_t)((scratchpad[1] << 8) | scratchpad[0]);

    // Конвертація у міліградуси Цельсія (цілочисельна математика)
    // 1 крок АЦП DS18B20 = 0.0625 °C (або 62.5 mC)
    *temp_mc = (int32_t)raw_temp * 625 / 10;

    return true;
}