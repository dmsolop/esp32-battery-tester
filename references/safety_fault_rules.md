# Safety & Fault Management Reference Rules

## 1. Bitmask Error Definitions (`system_types.h`)
- `ERR_OVER_TEMP` (0x01): Thermal limit exceeded (Heatsink > 85°C or Cell > user `thermal_limit_mc`).
- `ERR_OVER_CURRENT` (0x02): Discharge current exceeded hardware ceiling (> 30.0 A / 30,000,000 uA).
- `ERR_OVER_VOLTAGE` (0x04): Input voltage exceeded safe max (> 60.0 V / 60,000,000 uV).
- `ERR_THERMAL_RUNAWAY` (0x08): Rapid temperature gradient drop/spike ($dT/dt > 1^\circ\text{C/min}$ on Li-ion).
- `ERR_VOLTAGE_SAG` (0x10): Abnormal instant voltage drop upon load connect (e.g., > 500 mV in 1 ms indicating open circuit or fake cell).
- `ERR_TIMEOUT` (0x20): Execution time exceeded configured max `time_limit_s`.

## 2. Hardware Bypass & Emergency Sequence
1. **Hardware Level (ISR):** `ADC_ALERT_PIN` (IO47) triggers a falling-edge GPIO interrupt.
2. **Instant PWM Cutoff:** `adc_alert_isr_handler()` calls `pwm_driver_emergency_stop_isr()`, writing zeroes directly to LEDC registers to instantly kill the MOSFET gate drive.
3. **Safety Task Unblock:** ISR invokes `vTaskNotifyGiveFromISR()` to instantly wake up the `safety_task`.
4. **FSM Latching & Notification:** `safety_task` identifies the fault channel, sets `error_flags`, mutates state to `STATE_ERROR` in `system_state`, and sends a Direct Task Notification (`xTaskNotifyGive`) to `pid_control_task`.
5. **Audible Alarm (Buzzer):** The `safety_task` queues an asynchronous message to the Buzzer task to generate the `ALARM_CRITICAL` sound pattern.

## 3. Execution & Mutex Rules
- **Zero I2C Contention:** Never call I2C read functions (`adc_driver_read_*`) inside `safety_task`. 
- **SSOT Fetching:** Always fetch metrics via `system_state_get_metrics(ch, &metrics)` to ensure mutex safety.
- **Event-Driven Timing:** Use `ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100))` for the main loop. This acts as a 100ms periodic safety watchdog while allowing instant unblocking from ISRs.

## 4. Fault Acknowledge & Clearing
- All hardware and software faults are strictly latched. 
- A channel in `STATE_ERROR` can NEVER automatically transition back to `STATE_IDLE`. 
- The fault flag must be manually cleared via a physical UI interrupt (Encoder Button Click) handled by `ui_interface`.