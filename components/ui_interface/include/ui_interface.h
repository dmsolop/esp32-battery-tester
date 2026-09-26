#pragma once

#include "esp_err.h"

// Ініціалізація та запуск таски інтерфейсу користувача
esp_err_t ui_interface_init(void);
void ui_update_displays(void);