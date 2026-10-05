#include "temp_service.h"
#include "ds18b20.h"
#include "onewire.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <stddef.h>

static const char *TAG = "TEMP_SVC";
static gpio_num_t s_onewire_pins[4];

static int64_t s_last_conversion_time = 0;
static bool s_is_converting = false;

void temp_service_init(const gpio_num_t pins[4])
{
    for (int i = 0; i < 4; i++)
    {
        s_onewire_pins[i] = pins[i];
        onewire_init(s_onewire_pins[i]);
    }
    s_is_converting = false;
    s_last_conversion_time = 0;
    ESP_LOGI(TAG, "Initialized 4 independent OneWire buses");
}

void temp_service_auto_assign(uint8_t channel, temp_sensor_data_t *sensors)
{
    if (channel >= 4 || sensors == NULL)
        return;

    uint8_t rom[8];
    uint8_t sensor_idx = 0;
    gpio_num_t pin = s_onewire_pins[channel];

    onewire_reset_search();
    while (onewire_search(pin, rom) && sensor_idx < MAX_SENSORS_PER_CHANNEL)
    {
        // Читаємо Scratchpad для отримання ролі з EEPROM (байт T_L)
        if (onewire_reset(pin))
        {
            onewire_write_byte(pin, 0x55); // MATCH ROM
            for (int i = 0; i < 8; i++)
                onewire_write_byte(pin, rom[i]);
            onewire_write_byte(pin, 0xBE); // READ SCRATCHPAD

            uint8_t data[9];
            for (int i = 0; i < 9; i++)
                data[i] = onewire_read_byte(pin);

            uint8_t role_byte = data[3]; // T_L byte

            // Якщо роль не встановлена, ігноруємо або ставимо NONE
            sensor_role_t role = SENSOR_ROLE_NONE;
            if (role_byte == 0x01)
                role = SENSOR_ROLE_HEATSINK;
            else if (role_byte == 0x02)
                role = SENSOR_ROLE_CELL;

            if (role != SENSOR_ROLE_NONE)
            {
                for (int i = 0; i < 8; i++)
                    sensors[sensor_idx].rom[i] = rom[i];
                sensors[sensor_idx].role = role;
                sensors[sensor_idx].is_bound = true;
                sensor_idx++;
                ESP_LOGI(TAG, "CH%d: Bound sensor %02X... role %d", channel, rom[7], role);
            }
        }
    }
}

bool temp_service_trigger_conversion(void)
{
    if (s_is_converting)
        return false;

    // Запускаємо конвертацію паралельно на всіх 4 шинах
    for (int i = 0; i < 4; i++)
    {
        if (onewire_reset(s_onewire_pins[i]))
        {
            onewire_write_byte(s_onewire_pins[i], 0xCC); // SKIP ROM
            onewire_write_byte(s_onewire_pins[i], 0x44); // CONVERT T
        }
    }
    s_last_conversion_time = esp_timer_get_time();
    s_is_converting = true;
    return true;
}

bool temp_service_is_conversion_done(void)
{
    if (!s_is_converting)
        return false;

    if (esp_timer_get_time() - s_last_conversion_time >= 750000)
    {
        s_is_converting = false;
        return true;
    }
    return false;
}

void temp_service_read_sensors(uint8_t channel, temp_sensor_data_t *sensors)
{
    if (channel >= 4 || sensors == NULL)
        return;
    gpio_num_t pin = s_onewire_pins[channel];

    for (uint8_t i = 0; i < MAX_SENSORS_PER_CHANNEL; i++)
    {
        if (sensors[i].is_bound)
        {
            int32_t temp_mc;
            if (ds18b20_read_temperature(pin, sensors[i].rom, &temp_mc))
            {
                sensors[i].current_temp_mc = temp_mc;
            }
            else
            {
                ESP_LOGE(TAG, "CH%d CRC error on sensor role %d", channel, sensors[i].role);
            }
        }
    }
}