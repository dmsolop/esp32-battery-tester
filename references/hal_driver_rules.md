# HAL & Peripheral Driver Design Rules

## 1. Core Driver Principles
- **Stateless Execution:** HAL drivers must remain pure data fetchers. Never embed FSM state transition logic, battery chemistry rules, or load cutoff decisions inside driver functions.
- **I2C Mutex Protection:** Always acquire `s_i2c_mutex` during ADS1115 transactions to prevent bus contention between the control loop and UI rendering tasks.
- **Integer Micro-Math:** Perform all telemetry processing in integer micro-units ($\mu\text{V}$, $\mu\text{A}$, $\mu\text{W}$). Floating-point arithmetic is strictly forbidden in driver calculations to prevent CPU overhead.

## 2. ADS1115 ADC Configuration & Calculations
- **I2C Bus Configuration:** 100 kHz standard speed (`I2C_PWR_SDA`: IO6, `I2C_PWR_SCL`: IO7) to ensure noise immunity over twisted pair cabling.
- **Sampling Rate:** 128 SPS for stable DC measurement.
- **Dynamic PGA Scaling:**
  - **Voltage Channel:** Differential mode (Kelvin sensing), PGA set to $\pm 4.096\text{V}$ (Bits 11-9 = `001`).
  - **Current Channel:** Single-ended mode across shunt, PGA set to $\pm 0.256\text{V}$ (Bits 11-9 = `101`).
- **Ohm's Law Calculation (64-bit safe):**
  $$I_{\mu A} = (\text{uint32\_t})\left( \frac{(\text{uint64\_t})V_{shunt\_uv} \cdot 1000000}{R_{shunt\_micro\_ohm}} \right)$$
- **Zero-Offset Trimming:**
  $$V_{clean\_uv} = (V_{raw\_uv} > V_{offset\_uv}) ? (V_{raw\_uv} - V_{offset\_uv}) : 0$$
- **Hardware Bypass (ALERT/RDY):** The ADC comparator is programmed with critical thresholds. Upon limit breach, the `ADC_ALERT_PIN` (IO47) is pulled low, triggering an ESP32-S3 hardware ISR for instant PWM cutoff.

## 3. DS18B20 1-Wire Protocol & Timing
- **Reset Pulse:** Minimum 480 $\mu\text{s}$ low pulse followed by 15–60 $\mu\text{s}$ presence pulse response.
- **Non-Blocking Conversion Sequence:**
  1. Issue `0xCC` (Skip ROM) + `0x44` (Convert T).
  2. Record start timestamp `start_time_us = esp_timer_get_time()`.
  3. Return immediately; allow FreeRTOS tasks to execute without blocking delays.
  4. After $\ge 750000\text{ }\mu\text{s}$, read 9-byte Scratchpad from target devices.

## 4. Dallas CRC8 Polynomial Algorithm
- **Polynomial equation:** $X^8 + X^5 + X^4 + 1$ (Bit reverse representation: `0x8C`).
- Every 9-byte Scratchpad read **must** be validated against the 9th byte CRC. If `calculated_crc != scratchpad[8]`, discard the sample and retain the previous valid value to prevent PWM EMI noise corruption.

## 5. DS18B20 Scratchpad & EEPROM Map (Hardware Auto-Routing)
- Byte 0: Temperature LSB
- Byte 1: Temperature MSB
- Byte 2 ($T_H$ EEPROM): **Board / Channel ID** (`0x00` = CH0, `0x01` = CH1, `0xFF` = Unbound)
- Byte 3 ($T_L$ EEPROM): **Sensor Role** (`0x01` = `SENSOR_ROLE_CELL`, `0x02` = `SENSOR_ROLE_HEATSINK`)
- Byte 4: Configuration Register
- Bytes 5–7: Reserved
- Byte 8: CRC8 Byte