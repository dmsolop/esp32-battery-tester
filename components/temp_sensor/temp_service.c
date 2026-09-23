#include "temp_service.h"
#include "ds18b20.h"
#include "onewire.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "TEMP_SVC";
static gpio_num_t s_onewire_pin;

// Статичні змінні для збереження стану кінцевого автомата між ітераціями
static int64_t s_last_conversion_time = 0;
static bool s_is_converting = false;

static temp_sensor_data_t s_internal_sensors[MAX_SYSTEM_SENSORS];
static uint8_t s_sensor_count = 0;

void temp_service_init(gpio_num_t pin)
{
    s_onewire_pin = pin;
    onewire_init(pin);
    memset(s_internal_sensors, 0, sizeof(s_internal_sensors));
    s_sensor_count = 0;

    ESP_LOGI(TAG, "Scanning OneWire bus...");
    uint8_t rom[8];
    onewire_reset_search();

    while (onewire_search(pin, rom) && s_sensor_count < MAX_SYSTEM_SENSORS)
    {
        memcpy(s_internal_sensors[s_sensor_count].rom, rom, 8);
        s_internal_sensors[s_sensor_count].is_bound = true;

        // Базовий розподіл ролей (у релізній версії це підтягуватиметься з NVS або Kconfig)
        if (s_sensor_count == 0)
        {
            s_internal_sensors[s_sensor_count].role = SENSOR_ROLE_HEATSINK;
            s_internal_sensors[s_sensor_count].limit_temp_mc = 85000; // 85 C
        }
        else
        {
            s_internal_sensors[s_sensor_count].role = SENSOR_ROLE_CELL_LIION;
            s_internal_sensors[s_sensor_count].limit_temp_mc = 60000; // 60 C
        }

        ESP_LOGI(TAG, "Found Sensor %d: ROM %02X%02X...", s_sensor_count, rom[0], rom[1]);
        s_sensor_count++;
    }

    s_is_converting = false;
    s_last_conversion_time = 0;
}

void temp_service_process(temp_sensor_data_t *system_sensors, uint8_t *out_count)
{
    if (s_sensor_count == 0 || system_sensors == NULL)
    {
        if (out_count)
            *out_count = 0;
        return;
    }

    int64_t current_time = esp_timer_get_time(); // Час у мікросекундах

    // ФАЗА 1: Трансляційний запит на конвертацію (Broadcast)
    if (!s_is_converting)
    {
        if (onewire_reset(s_onewire_pin))
        {
            onewire_write_byte(s_onewire_pin, 0xCC); // Команда SKIP ROM
            onewire_write_byte(s_onewire_pin, 0x44); // Команда CONVERT T
            s_last_conversion_time = current_time;
            s_is_converting = true;
        }
        return; // Миттєве повернення керування у Task_Safety
    }

    // ФАЗА 2: Асинхронне очікування
    // 750000 мкс = 750 мс. Якщо час не вийшов, пропускаємо такти
    if (current_time - s_last_conversion_time < 750000)
    {
        return;
    }

    // ФАЗА 3: Зчитування результатів по кожній ROM-адресі
    for (uint8_t i = 0; i < s_sensor_count; i++)
    {
        int32_t temp_mc;
        // Функція ds18b20_read_temperature вже містить нашу валідацію CRC8
        if (ds18b20_read_temperature(s_onewire_pin, s_internal_sensors[i].rom, &temp_mc))
        {
            s_internal_sensors[i].current_temp_mc = temp_mc;
        }
    }

    // Оновлення зовнішнього масиву для Safety Monitor
    memcpy(system_sensors, s_internal_sensors, sizeof(temp_sensor_data_t) * s_sensor_count);
    if (out_count)
        *out_count = s_sensor_count;

    // Скидання прапорця для запуску нової конвертації на наступному циклі
    s_is_converting = false;
}