#include "onewire.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"

static uint8_t s_ROM_NO[8];
static uint8_t s_LastDiscrepancy;
static bool s_LastDeviceFlag;

void onewire_init(gpio_num_t pin)
{
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_pull_mode(pin, GPIO_PULLUP_ONLY);
}

bool onewire_reset(gpio_num_t pin)
{
    gpio_set_level(pin, 0);
    esp_rom_delay_us(480);
    gpio_set_level(pin, 1);
    esp_rom_delay_us(70);
    int presence = gpio_get_level(pin);
    esp_rom_delay_us(410);
    return (presence == 0);
}

void onewire_write_bit(gpio_num_t pin, uint8_t bit)
{
    gpio_set_level(pin, 0);
    if (bit)
    {
        esp_rom_delay_us(5);
        gpio_set_level(pin, 1);
        esp_rom_delay_us(60);
    }
    else
    {
        esp_rom_delay_us(60);
        gpio_set_level(pin, 1);
        esp_rom_delay_us(5);
    }
}

uint8_t onewire_read_bit(gpio_num_t pin)
{
    uint8_t bit = 0;
    gpio_set_level(pin, 0);
    esp_rom_delay_us(2);
    gpio_set_level(pin, 1);
    esp_rom_delay_us(10);
    bit = gpio_get_level(pin);
    esp_rom_delay_us(50);
    return bit;
}

void onewire_write_byte(gpio_num_t pin, uint8_t data)
{
    for (int i = 0; i < 8; i++)
    {
        onewire_write_bit(pin, data & 0x01);
        data >>= 1;
    }
}

uint8_t onewire_read_byte(gpio_num_t pin)
{
    uint8_t data = 0;
    for (int i = 0; i < 8; i++)
    {
        if (onewire_read_bit(pin))
        {
            data |= (1 << i);
        }
    }
    return data;
}

void onewire_reset_search(void)
{
    s_LastDiscrepancy = 0;
    s_LastDeviceFlag = false;
    for (int i = 0; i < 8; i++)
    {
        s_ROM_NO[i] = 0;
    }
}

bool onewire_search(gpio_num_t pin, uint8_t *newAddr)
{
    uint8_t id_bit_number = 1;
    uint8_t last_zero = 0;
    uint8_t rom_byte_number = 0;
    bool search_result = false;
    uint8_t id_bit, cmp_id_bit;
    uint8_t search_direction;

    if (!s_LastDeviceFlag)
    {
        if (!onewire_reset(pin))
        {
            onewire_reset_search();
            return false;
        }

        onewire_write_byte(pin, 0xF0);

        while (id_bit_number < 65)
        {
            id_bit = onewire_read_bit(pin);
            cmp_id_bit = onewire_read_bit(pin);

            if ((id_bit == 1) && (cmp_id_bit == 1))
            {
                break;
            }
            else
            {
                if (id_bit != cmp_id_bit)
                {
                    search_direction = id_bit;
                }
                else
                {
                    if (id_bit_number < s_LastDiscrepancy)
                    {
                        search_direction = ((s_ROM_NO[rom_byte_number] & (1 << ((id_bit_number - 1) % 8))) > 0);
                    }
                    else
                    {
                        search_direction = (id_bit_number == s_LastDiscrepancy);
                    }
                    if (search_direction == 0)
                    {
                        last_zero = id_bit_number;
                    }
                }

                if (search_direction == 1)
                {
                    s_ROM_NO[rom_byte_number] |= (1 << ((id_bit_number - 1) % 8));
                }
                else
                {
                    s_ROM_NO[rom_byte_number] &= ~(1 << ((id_bit_number - 1) % 8));
                }

                onewire_write_bit(pin, search_direction);
                id_bit_number++;

                if (((id_bit_number - 1) % 8) == 0)
                {
                    rom_byte_number++;
                }
            }
        }

        if (id_bit_number >= 65)
        {
            s_LastDiscrepancy = last_zero;
            if (s_LastDiscrepancy == 0)
            {
                s_LastDeviceFlag = true;
            }
            search_result = true;
        }
    }

    if (!search_result || !s_ROM_NO[0])
    {
        onewire_reset_search();
        search_result = false;
    }
    else
    {
        for (int i = 0; i < 8; i++)
        {
            newAddr[i] = s_ROM_NO[i];
        }
    }

    return search_result;
}