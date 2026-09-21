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
    ctx->prev_error = 0.0f;
    ctx->current_output = ctx->out_min; // Гарантоване скидання ШІМ до мінімуму
}

float pid_service_compute(pid_context_t *ctx, float setpoint, float measured_value)
{
    if (ctx == NULL)
        return 0.0f;

    // 1. Розрахунок відхилення (Error)
    float error = setpoint - measured_value;

    // 2. Інтегральна складова та захист від насичення (Anti-windup)
    ctx->integral += error;

    if (ctx->integral > 500000.0f)
        ctx->integral = 500000.0f;
    if (ctx->integral < -500000.0f)
        ctx->integral = -500000.0f;

    // 3. Диференціальна складова
    float derivative = error - ctx->prev_error;

    // 4. Загальний вихід ПІД-регулятора
    float output = (ctx->kp * error) + (ctx->ki * ctx->integral) + (ctx->kd * derivative);

    ctx->prev_error = error;

    // 5. Коригування ШІМ та обмеження (Clamping)
    ctx->current_output += output;

    if (ctx->current_output > ctx->out_max)
        ctx->current_output = ctx->out_max;
    if (ctx->current_output < ctx->out_min)
        ctx->current_output = ctx->out_min;

    return ctx->current_output;
}