#include "temp_service.h"
#include "ds18b20.h"
#include "onewire.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <stddef.h>

static const char *TAG = "TEMP_SVC";
static gpio_num_t s_onewire_pin;

// Змінні мікро-кінцевого автомата
static int64_t s_last_conversion_time = 0;
static bool s_is_converting = false;

void temp_service_init(gpio_num_t pin)
{
    s_onewire_pin = pin;
    onewire_init(pin);
    s_is_converting = false;
    s_last_conversion_time = 0;
    ESP_LOGI(TAG, "Temp service hardware initialized on pin %d", pin);
}

uint8_t temp_service_scan_bus(uint8_t discovered_roms[][8], uint8_t max_roms)
{
    uint8_t count = 0;
    uint8_t rom[8];

    onewire_reset_search();
    while (onewire_search(s_onewire_pin, rom) && count < max_roms)
    {
        for (int i = 0; i < 8; i++)
        {
            discovered_roms[count][i] = rom[i];
        }
        count++;
    }
    return count;
}

void temp_service_process(temp_sensor_data_t *sensors, uint8_t count)
{
    if (count == 0 || sensors == NULL)
    {
        return;
    }

    int64_t current_time = esp_timer_get_time();

    // ФАЗА 1: Трансляційний запит на конвертацію
    if (!s_is_converting)
    {
        // Перевіряємо, чи є хоча б один прив'язаний датчик
        bool needs_conversion = false;
        for (uint8_t i = 0; i < count; i++)
        {
            if (sensors[i].is_bound)
            {
                needs_conversion = true;
                break;
            }
        }

        if (!needs_conversion)
            return; // Немає сенсу смикати шину

        if (onewire_reset(s_onewire_pin))
        {
            onewire_write_byte(s_onewire_pin, 0xCC); // SKIP ROM
            onewire_write_byte(s_onewire_pin, 0x44); // CONVERT T
            s_last_conversion_time = current_time;
            s_is_converting = true;
        }
        return;
    }

    // ФАЗА 2: Асинхронне очікування
    if (current_time - s_last_conversion_time < 750000)
    {
        return;
    }

    // ФАЗА 3: Зчитування результатів виключно для прив'язаних датчиків
    for (uint8_t i = 0; i < count; i++)
    {
        if (sensors[i].is_bound)
        {
            int32_t temp_mc;
            if (ds18b20_read_temperature(s_onewire_pin, sensors[i].rom, &temp_mc))
            {
                // Оновлюємо ЄДИНЕ поле, за яке відповідає цей сервіс
                sensors[i].current_temp_mc = temp_mc;
            }
            else
            {
                ESP_LOGE(TAG, "Failed to read or CRC error on bound sensor index %d", i);
            }
        }
    }

    s_is_converting = false;
}