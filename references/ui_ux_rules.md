# UI/UX & Display Systems Design Rules

## 1. Dual Display Architecture & Address Mapping
- **Bus Assignment:** Dedicated `I2C_NUM_1` bus (400 kHz Fast Mode).
- **Physical Displays:** Two 0.96" SSD1306 OLED screens (128x64 resolution each).
- **Hardware Rotation:** Panels are physically mounted upside down to simplify PCB routing; initialize with `esp_lcd_panel_mirror` to rotate 180 degrees.
- **Left Screen (Master Overview):** I2C Address `0x3C` (Virtual coordinates $X: 0 \rightarrow 127, Y: 0 \rightarrow 63$).
- **Right Screen (Detail & Settings):** I2C Address `0x3D` (Virtual coordinates $X: 128 \rightarrow 255, Y: 0 \rightarrow 63$).
- **Memory Structure:** Total virtual framebuffer size is 2048 bytes (2 KB RAM).

## 2. PCNT Quadrature Encoder & Input Configuration
- **Hardware Module:** ESP32-S3 PCNT (Pulse Count Controller), Unit 0.
- **Glitch Filter:** Enabled with a threshold of 1000 APB clock cycles (~12.5 $\mu\text{s}$) to eliminate contact bounce noise without CPU overhead.
- **Safe Wraparound Math:** Navigation index updates MUST use modulo arithmetic to prevent negative index array crashes.
- **Encoder Button:** Handled via standard GPIO interrupt with RTOS software debounce.

## 3. Master-Detail Navigation Hierarchy (Passive Observer)
- **Strict Data-Driven UI:** The UI layer is purely a passive viewer. It MUST NOT cache, duplicate, or mutate telemetry metrics locally. All data must be fetched directly from `system_state`.
- **Master View (Left OLED):** Summary rows for CH0-CH3 showing Chemistry, State (IDLE/RUN/ERR), Voltage ($V$), and Current ($A$).
- **Detail View (Right OLED):**
  - `UI_STATE_CHANNEL_DETAIL`: Deep telemetry ($Ah$, $Wh$, Temperature, DCIR $m\Omega$, elapsed time).
  - `UI_STATE_SETTINGS`: Interactive menu for chemistry selection, target current, and limits.
  - `UI_STATE_ALARM_OVERLAY`: High-priority modal overlay (inverted black-on-white flashing box) showing Fault Name and `[ CLICK TO ACK ]`.

## 4. Audio-Visual UX Feedback (Buzzer)
- **Navigation Feedback:** Short, discrete "tick" (e.g., 10 ms pulse) for every encoder detent step or button press.
- **Alarm Feedback:** High-priority, alternating two-tone sequence triggered via non-blocking task notification when `metrics.error_flags != 0`.

## 5. Immediate Mode GUI (IMGUI) Pipeline
1. Clear Virtual Framebuffer (`memset 0x00`, 2048 bytes).
2. Fetch active metrics via `system_state_get_metrics()`.
3. Render Master Screen Elements ($X: 0..127$).
4. Render Active Detail/Menu Screen Elements ($X: 128..255$).
5. Check `error_flags`; if fault detected, render Inverted Overlay Box.
6. Flush Left Panel (`0x3C`) $\rightarrow$ Flush Right Panel (`0x3D`).