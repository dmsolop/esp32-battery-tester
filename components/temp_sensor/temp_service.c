#include "temp_service.h"
#include "ds18b20.h"
#include "onewire.h"
#include "system_state.h"
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
    gpio_num_t pin = s_onewire_pins[channel];

    // Зчитуємо поточні налаштування для отримання thermal_limit_mc
    channel_metrics_t cur_metrics;
    int32_t cell_limit_mc = 60000;
    if (system_state_get_metrics(channel, &cur_metrics) == ESP_OK)
    {
        cell_limit_mc = cur_metrics.settings.thermal_limit_mc;
    }

    // Спочатку скидаємо старі прив'язки
    for (int i = 0; i < MAX_SENSORS_PER_CHANNEL; i++)
    {
        sensors[i].is_bound = false;
        sensors[i].role = SENSOR_ROLE_NONE;
    }

    typedef struct
    {
        uint8_t rom[8];
        uint8_t role_byte;
        sensor_role_t role;
    } discovered_sensor_t;

    discovered_sensor_t discovered[MAX_SENSORS_PER_CHANNEL];
    uint8_t disc_count = 0;

    onewire_reset_search();
    while (onewire_search(pin, rom) && disc_count < MAX_SENSORS_PER_CHANNEL)
    {
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

            sensor_role_t role = SENSOR_ROLE_NONE;
            if (role_byte == 0x01)
                role = SENSOR_ROLE_HEATSINK;
            else if (role_byte == 0x02)
                role = SENSOR_ROLE_CELL;

            for (int i = 0; i < 8; i++)
                discovered[disc_count].rom[i] = rom[i];
            discovered[disc_count].role_byte = role_byte;
            discovered[disc_count].role = role;
            disc_count++;
        }
    }

    uint8_t hs_count = 0;
    uint8_t cell_count = 0;
    uint8_t sensor_idx = 0;

    // Фаза 1: Прив'язка тих, у кого вже записана валідна роль у EEPROM
    for (uint8_t i = 0; i < disc_count && sensor_idx < MAX_SENSORS_PER_CHANNEL; i++)
    {
        if (discovered[i].role == SENSOR_ROLE_HEATSINK && hs_count < 2)
        {
            for (int r = 0; r < 8; r++)
                sensors[sensor_idx].rom[r] = discovered[i].rom[r];
            sensors[sensor_idx].role = SENSOR_ROLE_HEATSINK;
            sensors[sensor_idx].limit_temp_mc = 85000;
            sensors[sensor_idx].is_bound = true;
            hs_count++;
            sensor_idx++;
            discovered[i].role = SENSOR_ROLE_NONE; // Позначено як призначений
            ESP_LOGI(TAG, "CH%d: Bound existing HEATSINK sensor %02X%02X...", channel, rom[0], rom[7]);
        }
        else if (discovered[i].role == SENSOR_ROLE_CELL && cell_count < 1)
        {
            for (int r = 0; r < 8; r++)
                sensors[sensor_idx].rom[r] = discovered[i].rom[r];
            sensors[sensor_idx].role = SENSOR_ROLE_CELL;
            sensors[sensor_idx].limit_temp_mc = cell_limit_mc;
            sensors[sensor_idx].is_bound = true;
            cell_count++;
            sensor_idx++;
            discovered[i].role = SENSOR_ROLE_NONE; // Позначено як призначений
            ESP_LOGI(TAG, "CH%d: Bound existing CELL sensor %02X%02X...", channel, rom[0], rom[7]);
        }
    }

    // Фаза 2: Автоматичний фолбек для нових/заводських датчиків (role_byte != 0x01/0x02 або перевищення)
    for (uint8_t i = 0; i < disc_count && sensor_idx < MAX_SENSORS_PER_CHANNEL; i++)
    {
        if (discovered[i].role_byte != 0x01 && discovered[i].role_byte != 0x02)
        {
            if (hs_count < 2)
            {
                // Призначаємо як HEATSINK
                for (int r = 0; r < 8; r++)
                    sensors[sensor_idx].rom[r] = discovered[i].rom[r];
                sensors[sensor_idx].role = SENSOR_ROLE_HEATSINK;
                sensors[sensor_idx].limit_temp_mc = 85000;
                sensors[sensor_idx].is_bound = true;
                hs_count++;
                sensor_idx++;
                ESP_LOGI(TAG, "CH%d: Auto-fallback HEATSINK (HS%d) sensor %02X%02X...", channel, hs_count, discovered[i].rom[0], discovered[i].rom[7]);
            }
            else if (cell_count < 1)
            {
                // Призначаємо як CELL
                for (int r = 0; r < 8; r++)
                    sensors[sensor_idx].rom[r] = discovered[i].rom[r];
                sensors[sensor_idx].role = SENSOR_ROLE_CELL;
                sensors[sensor_idx].limit_temp_mc = cell_limit_mc;
                sensors[sensor_idx].is_bound = true;
                cell_count++;
                sensor_idx++;
                ESP_LOGI(TAG, "CH%d: Auto-fallback CELL sensor %02X%02X...", channel, discovered[i].rom[0], discovered[i].rom[7]);
            }
        }
    }

    ESP_LOGI(TAG, "CH%d: Auto-assign completed. Total bound: %d (HS: %d, CELL: %d)", channel, sensor_idx, hs_count, cell_count);
}

esp_err_t temp_service_bind_sensor_role(uint8_t channel, uint8_t sensor_idx, sensor_role_t new_role)
{
    if (channel >= 4 || sensor_idx >= MAX_SENSORS_PER_CHANNEL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    channel_metrics_t metrics;
    esp_err_t err = system_state_get_metrics(channel, &metrics);
    if (err != ESP_OK)
    {
        return err;
    }

    if (!metrics.temp_sensors[sensor_idx].is_bound)
    {
        return ESP_ERR_NOT_FOUND;
    }

    gpio_num_t pin = s_onewire_pins[channel];
    const uint8_t *rom = metrics.temp_sensors[sensor_idx].rom;

    // Читаємо Scratchpad для збереження існуючого байта конфігурації
    uint8_t cfg_byte = 0x7F; // 12-біт за замовчуванням
    if (onewire_reset(pin))
    {
        onewire_write_byte(pin, 0x55);
        for (int i = 0; i < 8; i++)
            onewire_write_byte(pin, rom[i]);
        onewire_write_byte(pin, 0xBE);
        uint8_t data[9];
        for (int i = 0; i < 9; i++)
            data[i] = onewire_read_byte(pin);
        cfg_byte = data[4];
    }

    uint8_t th_byte = channel;
    uint8_t tl_byte = (new_role == SENSOR_ROLE_HEATSINK) ? 0x01 : ((new_role == SENSOR_ROLE_CELL) ? 0x02 : 0x00);

    // Запис у Scratchpad та копіювання у вбудовану EEPROM
    ds18b20_write_scratchpad(pin, rom, th_byte, tl_byte, cfg_byte);
    ds18b20_copy_scratchpad(pin, rom);

    // Оновлення локальної ролі та ліміту в пам'яті
    metrics.temp_sensors[sensor_idx].role = new_role;
    if (new_role == SENSOR_ROLE_HEATSINK)
    {
        metrics.temp_sensors[sensor_idx].limit_temp_mc = 85000;
    }
    else if (new_role == SENSOR_ROLE_CELL)
    {
        metrics.temp_sensors[sensor_idx].limit_temp_mc = metrics.settings.thermal_limit_mc;
    }
    else
    {
        metrics.temp_sensors[sensor_idx].limit_temp_mc = 85000;
    }

    system_state_set_metrics(channel, &metrics);
    system_state_update_sensor_limits(channel);

    ESP_LOGI(TAG, "CH%d Sensor[%d] role changed to %d and written to EEPROM", channel, sensor_idx, new_role);
    return ESP_OK;
}

esp_err_t temp_service_rescan(uint8_t channel)
{
    if (channel >= 4)
    {
        return ESP_ERR_INVALID_ARG;
    }

    channel_metrics_t metrics;
    esp_err_t err = system_state_get_metrics(channel, &metrics);
    if (err != ESP_OK)
    {
        return err;
    }

    temp_service_auto_assign(channel, metrics.temp_sensors);
    system_state_set_metrics(channel, &metrics);
    system_state_update_sensor_limits(channel);

    ESP_LOGI(TAG, "CH%d 1-Wire manual rescan complete", channel);
    return ESP_OK;
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