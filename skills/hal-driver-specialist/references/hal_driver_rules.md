# HAL & Peripheral Driver Design Rules

## 1. ADS1115 ADC Configuration & Calculations
- **I2C Bus Configuration:** 100 kHz standard / forced speed to ensure noise immunity over twisted pair cabling.
- **Sampling Rate & Gain:** 128 SPS, Programmable Gain Amplifier (PGA) set according to full-scale voltage/current requirements.
- **Ohm's Law Calculation:**
  $$I_{\mu A} = (\text{uint32\_t})\left( \frac{(\text{uint64\_t})V_{shunt\_uv} \cdot 1000000}{R_{shunt\_micro\_ohm}} \right)$$
- **Zero-Offset Trimming:**
  $$V_{clean\_uv} = (V_{raw\_uv} > V_{offset\_uv}) ? (V_{raw\_uv} - V_{offset\_uv}) : 0$$

## 2. DS18B20 1-Wire Protocol & Timing
- **Reset Pulse:** Minimum 480 $\mu\text{s}$ low pulse followed by 15–60 $\mu\text{s}$ presence pulse response.
- **Non-Blocking Conversion Sequence:**
  1. Issue `0xCC` (Skip ROM) + `0x44` (Convert T).
  2. Record start timestamp `start_time_us = esp_timer_get_time()`.
  3. Return immediately; allow FreeRTOS tasks to execute.
  4. After $\ge 750000\text{ }\mu\text{s}$, read 9-byte Scratchpad from target devices.

## 3. Dallas CRC8 Polynomial Algorithm
Polynomial equation: $X^8 + X^5 + X^4 + 1$ (Bit reverse representation: `0x8C`).
- Every 9-byte Scratchpad read **must** be validated against the 9th byte CRC. If `calculated_crc != scratchpad[8]`, discard the sample to prevent PWM noise corruption.

## 4. DS18B20 Scratchpad & EEPROM Map
- Byte 0: Temperature LSB
- Byte 1: Temperature MSB
- Byte 2 ($T_H$ EEPROM): **Board / Channel ID** (e.g., `0x00` = CH0, `0x01` = CH1, `0xFF` = Unbound)
- Byte 3 ($T_L$ EEPROM): **Sensor Role** (`0x01` = SENSOR_ROLE_CELL, `0x02` = SENSOR_ROLE_HEATSINK)
- Byte 4: Configuration Register
- Bytes 5–7: Reserved
- Byte 8: CRC8 Byte