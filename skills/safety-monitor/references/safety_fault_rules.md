# Safety & Fault Management Reference Rules

## 1. Bitmask Error Definitions (`system_types.h`)
- `ERR_OVER_TEMP` (0x01): Thermal limit exceeded (Heatsink > 85°C or Cell > user thermal_limit_mc)[cite: 1].
- `ERR_OVER_CURRENT` (0x02): Discharge current exceeded hard ceiling (> 5.0 A / 5,000,000 uA)[cite: 1].
- `ERR_OVER_VOLTAGE` (0x04): Input voltage exceeded safe max (> 20.0 V / 20,000,000 uV)[cite: 1].
- `ERR_THERMAL_RUNAWAY` (0x08): Rapid temperature gradient drop/spike (dT/dt > 1°C/min on Li-ion).
- `ERR_VOLTAGE_SAG` (0x10): Abnormal instant voltage drop upon load connect (open circuit/bad cell).
- `ERR_TIMEOUT` (0x20): Execution time exceeded configured max time_limit_s.

## 2. Hardware Bypass & Emergency Sequence
1. **Hardware Level (ISR):** `ADC_ALERT_PIN` triggers falling-edge GPIO interrupt[cite: 1].
2. **Instant PWM Cutoff:** `adc_alert_isr_handler()` calls `pwm_driver_emergency_stop_isr()`, writing zeroes directly to LEDC registers.
3. **Safety Task Unblock:** ISR invokes `vTaskNotifyGiveFromISR()` to instantly wake up `safety_task`[cite: 1].
4. **FSM Latching:** `safety_task` identifies fault channel, sets `error_flags`, mutates state to `STATE_ERROR` in `system_state`[cite: 1], and sends Direct Task Notification (`xTaskNotifyGive`) to `pid_control_task`[cite: 1].

## 3. Execution & Mutex Rules
- Never call I2C read functions (`adc_driver_read_*`) inside `safety_task`.
- Always fetch metrics via `system_state_get_metrics(ch, &metrics)`.
- Use `ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100))` for event-driven / 100ms dual timing loop.