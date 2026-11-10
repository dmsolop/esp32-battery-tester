#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        BUZZER_EVENT_CLICK = 0,
        BUZZER_EVENT_CONFIRM,
        BUZZER_EVENT_WARNING,
        BUZZER_EVENT_TEST_DONE,
        BUZZER_EVENT_ERROR
    } buzzer_event_t;

    /**
     * @brief Ініціалізація сервісу звукової індикації (бузера) на базі MCPWM
     * @return ESP_OK у разі успіху
     */
    esp_err_t buzzer_service_init(void);

    /**
     * @brief Неблокуючий запит на програвання звукової події через FreeRTOS чергу
     * @param event Тип звукової події
     */
    void buzzer_play(buzzer_event_t event);

    /**
     * @brief Встановлення гучності бузера (0..100%), збереження в NVS ("storage", "bz_vol")
     * @param volume_pct Гучність у відсотках (0..100)
     */
    void buzzer_set_volume(uint8_t volume_pct);

    /**
     * @brief Отримання поточної гучності бузера
     * @return Гучність у відсотках (0..100)
     */
    uint8_t buzzer_get_volume(void);

#ifdef __cplusplus
}
#endif
