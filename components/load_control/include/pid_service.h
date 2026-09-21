#pragma once

typedef struct
{
    float kp;
    float ki;
    float kd;
    float integral;
    float prev_error;
    float current_output; // Внутрішня пам'ять поточного рівня ШІМ
    float out_min;        // Мінімальний ліміт ШІМ
    float out_max;        // Максимальний ліміт ШІМ
} pid_context_t;

// Ініціалізація контексту регулятора із заданими коефіцієнтами та лімітами
void pid_service_init(pid_context_t *ctx, float kp, float ki, float kd, float out_min, float out_max);

// Миттєве скидання накопиченої помилки та обнулення виходу ШІМ
void pid_service_reset(pid_context_t *ctx);

// Головна функція розрахунку. Повертає готове значення керуючого впливу
float pid_service_compute(pid_context_t *ctx, float setpoint, float measured_value);