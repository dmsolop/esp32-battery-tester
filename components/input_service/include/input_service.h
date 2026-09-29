#pragma once

#include "esp_err.h"
#include <stdbool.h>

// Ініціалізація апаратних ресурсів (PCNT, GPIO)
esp_err_t input_service_init(void);

// Зчитування поточного стану енкодера та кнопок.
// Викликається періодично з UI-таски.
void input_service_read(int *enc_diff, bool *enc_btn_clicked);
