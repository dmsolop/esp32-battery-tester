#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Селектор каналів вимірювання струму на підлеглій платі ADS1115
    typedef enum
    {
        ADC_CURRENT_SENSE_Q1 = 0, // AIN0 (0xCB83): струм плеча Q1
        ADC_CURRENT_SENSE_Q2 = 1, // AIN1 (0xDB83): струм плеча Q2
    } adc_current_channel_t;

    // Ініціалізація шини I2C та драйвера АЦП
    esp_err_t adc_driver_init(void);

    // Зчитування напруги (у мікровольтах) для заданого каналу (AIN2-AIN3 диференційно)
    esp_err_t adc_driver_read_voltage(uint8_t channel, uint32_t *voltage_uv);

    // Зчитування струму (у мікроамперах) для заданого каналу
    // За замовчуванням (або для сумісності з поточним макетом) повертає сумарний струм або активне плече
    esp_err_t adc_driver_read_current(uint8_t channel, uint32_t *current_ua, int32_t *pid_current_ua);

    // Зчитування струму конкретного плеча (AIN0: Q1 або AIN1: Q2)
    esp_err_t adc_driver_read_current_branch(uint8_t channel, adc_current_channel_t branch, uint32_t *current_ua, int32_t *pid_current_ua);

    // Зчитування детальної телеметрії обох плечей та їх суми
    esp_err_t adc_driver_read_current_dual(uint8_t channel, uint32_t *i_q1_ua, uint32_t *i_q2_ua, uint32_t *total_ua, int32_t *pid_total_ua);

    // Встановлення конфігурації кількость активних плечей вимірювання струму (false = одне плече для макету, true = обидва плеча для slave_board)
    void adc_driver_set_dual_branch_mode(uint8_t channel, bool enable_dual);

    // Встановлення напруги для тестування каналу
    void adc_driver_set_mock_voltage(uint8_t channel, uint32_t voltage_uv);

    // Встановлення струму для тестування каналу
    void adc_driver_set_mock_current(uint8_t channel, uint32_t current_ua);

    // Встановлення нульового зміщення для калібрування
    void adc_driver_set_zero_offset(uint8_t channel, int32_t v_off_uv, int32_t i_off_ua);

    // Встановлення індивідуального зміщення нуля для конкретного плеча струму
    void adc_driver_set_zero_offset_branch(uint8_t channel, adc_current_channel_t branch, int32_t i_off_ua);

    // Реєстрація таски безпеки для отримання сповіщень про апаратні аварії
    void adc_driver_register_safety_task(TaskHandle_t task);

#ifdef __cplusplus
}
#endif