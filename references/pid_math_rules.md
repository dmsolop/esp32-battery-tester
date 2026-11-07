# Control Systems Math & Implementation Rules

## 1. Time-Aware PID Controller Equations
To eliminate FreeRTOS task scheduling jitter, PID terms must be calculated using exact microsecond deltas (`dt_us = current_time - last_time`):

$$\Delta t_{sec} = \frac{dt\_us}{1000000.0f}$$

- **Error Term:** $e(t) = I_{target\_ua} - I_{measured\_ua}$
- **Proportional Term:** $P = K_p \cdot e(t)$
- **Integral Term with Dynamic Anti-Windup:**
  Only accumulate integral if PWM output is not saturated (0 < PWM < 4095), or if the error sign pulls it away from saturation:
  $$I_{acc} = I_{acc} + (e(t) \cdot \Delta t_{sec})$$
  $$I_{acc} = \text{clamp}(I_{acc}, -I_{limit}, +I_{limit})$$
  $$I = K_i \cdot I_{acc}$$
- **Derivative Term (Derivative on Measurement):**
  To prevent derivative kick during target setpoint changes, differentiate the process variable, not the error:
  $$D = -K_d \cdot \frac{I_{measured\_ua}(t) - I_{measured\_ua}(t-1)}{\Delta t_{sec}}$$
- **Output Scaling to 12-Bit PWM (Strict Positional Form — NO Double Integration):**
  $$\text{PWM\_Duty} = \text{clamp}\left(\text{round}(P + I + D), 0, 4095\right)$$

## 2. Strict Integer Numerical Integration (Capacity & Energy)
Floating-point logic ($\Delta t_{sec}$) is allowed for the PID FPU calculations, but strictly forbidden for long-term telemetry accumulation to prevent floating-point drift.

- **Current Calculation from Shunt (Zero-Loss Integer Math):**
  $$I_{\mu A} = \left(\text{uint32\_t}\right)\left(\frac{(\text{uint64\_t})\text{raw\_adc} \cdot 15625}{2 \cdot R_{shunt\_mohm}}\right)$$
- **Accumulated Charge (Pure Integer):**
  $$\text{accumulated\_uas} += \frac{(\text{uint64\_t})I_{measured\_ua} \cdot dt\_us}{1000000}$$
  $$\text{capacity\_mah} = (\text{uint32\_t})\left(\frac{\text{accumulated\_uas}}{3600}\right)$$
- **Accumulated Energy (Pure Integer):**
  $$P_{\mu W} = \frac{(\text{uint64\_t})V_{measured\_uv} \cdot I_{measured\_ua}}{1000000}$$
  $$\text{accumulated\_uws} += \frac{(\text{uint64\_t})P_{\mu W} \cdot dt\_us}{1000000}$$
  $$\text{energy\_mwh} = (\text{uint32\_t})\left(\frac{\text{accumulated\_uws}}{3600}\right)$$

## 3. Dynamic DCIR Pulse Math
Calculated via a periodic 100 ms current pulse:
1. Sample baseline $V_1, I_1$ in steady state (`STATE_DISCHARGING`).
2. Apply high-current pulse $I_2$ for $100\text{ ms}$.
3. Sample pulse $V_2, I_2$ at $90\text{ ms}$.
4. Calculate internal resistance in micro-ohms and return milliohms:
$$R_{ir\_mohm} = \frac{(V_1 - V_2)_{\mu V} \cdot 1000}{(I_2 - I_1)_{\mu A}}$$

## 4. Exponential Moving Average (EMA) Filtering
- **PID Control Path:** Apply EMA filtering ($\alpha = 0.2$) to raw ADC readings to filter out PWM switching noise:
  $$Y_t = \left(0.2 \cdot X_t\right) + \left(0.8 \cdot Y_{t-1}\right)$$
- **Fast Safety Path:** Pass unfiltered raw ADC readings directly to `safety_monitor` to ensure instant OCP/OVP response without phase delay.

## 5. Hardware LEDC PWM Configuration
- **Frequency:** $19531\text{ Hz}$ ($\approx 20\text{ kHz}$).
- **Resolution:** 12-bit ($2^{12} - 1 = 4095$ levels).
- **Analog DAC Filter (Option B2):** Two-stage passive RC filter with integrated voltage divider ($R_1 = 10\text{ k}\Omega$ for breadboard $0.27\ \Omega$ shunt or $33\text{ k}\Omega$ for $5\text{ m}\Omega$ shunt, $R_{div} = 1.0\text{ k}\Omega \parallel C_1 = 100\text{ nF}$, $R_2 = 4.7\text{ k}\Omega\dots 10\text{ k}\Omega$, $C_2 = 100\text{ nF}$), providing $<0.6\text{ mV}$ ripple and $<3\text{ ms}$ settling time without Op-Amp zero-offset errors.
- **Emergency Abort:** Immediate clear of LEDC duty register (`ledc_set_duty(..., 0)` followed by `ledc_update_duty(...)`).
