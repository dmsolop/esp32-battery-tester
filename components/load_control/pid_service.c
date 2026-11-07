#include "pid_service.h"
#include <stddef.h>

void pid_service_init(pid_context_t *ctx, float kp, float ki, float kd, float out_min, float out_max)
{
    if (ctx == NULL)
        return;

    ctx->kp = kp;
    ctx->ki = ki;
    ctx->kd = kd;
    ctx->out_min = out_min;
    ctx->out_max = out_max;

    // Одразу ініціалізуємо безпечний нульовий стан
    pid_service_reset(ctx);
}

void pid_service_reset(pid_context_t *ctx)
{
    if (ctx == NULL)
        return;

    ctx->integral = 0.0f;
    ctx->prev_measured = 0.0f;
    ctx->prev_measured_valid = false;
    ctx->current_output = ctx->out_min; // Гарантоване скидання ШІМ до мінімуму
}

float pid_service_compute(pid_context_t *ctx, float setpoint, float measured_value, float dt_sec)
{
    if (ctx == NULL)
        return 0.0f;

    if (dt_sec <= 0.0f)
        return ctx->current_output;

    // 1. Розрахунок похибки (Error)
    float error = setpoint - measured_value;

    // 2. Пропорційна складова (Proportional Term)
    float p_term = ctx->kp * error;

    // 3. Диференціальна складова за вимірюванням (Derivative-on-Measurement)
    // Усуває "диференціальний удар" (derivative kick) при стрибку уставки setpoint:
    // d(error)/dt = d(setpoint - y)/dt = -dy/dt при const setpoint
    float d_term = 0.0f;
    if (ctx->prev_measured_valid)
    {
        float d_measured = (measured_value - ctx->prev_measured) / dt_sec;
        d_term = -ctx->kd * d_measured;
    }
    else
    {
        ctx->prev_measured_valid = true;
    }
    ctx->prev_measured = measured_value;

    // 4. Попереднє обчислення ненасиченого виходу для динамічного Anti-Windup
    // Перевіряємо, чи дозволено накопичення інтеграла (clamping anti-windup)
    float tentative_integral = ctx->integral + (error * dt_sec);

    // Розрахунок передбачуваного виходу з урахуванням Ki
    float tentative_output = p_term + (ctx->ki * tentative_integral) + d_term;

    // Динамічний Anti-Windup:
    // Інтегруємо лише якщо вихід не в насиченні, АБО якщо знак похибки
    // виводить систему із зони насичення (від'ємна похибка при верхньому насиченні,
    // або додатна похибка при нижньому насиченні).
    bool saturated_high = (tentative_output >= ctx->out_max);
    bool saturated_low = (tentative_output <= ctx->out_min);

    if (saturated_high)
    {
        if (error < 0.0f)
        {
            ctx->integral = tentative_integral;
        }
    }
    else if (saturated_low)
    {
        if (error > 0.0f)
        {
            ctx->integral = tentative_integral;
        }
    }
    else
    {
        ctx->integral = tentative_integral;
    }

    // Додатковий жорсткий ліміт для захисту від переповнення інтеграла
    if (ctx->ki > 0.0f)
    {
        float max_integral = ctx->out_max / ctx->ki;
        if (ctx->integral > max_integral)
            ctx->integral = max_integral;
        else if (ctx->integral < -max_integral)
            ctx->integral = -max_integral;
    }

    float i_term = ctx->ki * ctx->integral;

    // 5. Суворе позиційне обчислення виходу (Strict Positional Form: output = P + I + D)
    // Жодного подвійного інтегрування (current_output += output вилучено)
    float output = p_term + i_term + d_term;

    // 6. Обмеження діапазону (Clamping [out_min, out_max])
    if (output > ctx->out_max)
        output = ctx->out_max;
    if (output < ctx->out_min)
        output = ctx->out_min;

    ctx->current_output = output;
    return ctx->current_output;
}