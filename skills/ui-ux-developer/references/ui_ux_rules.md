# UI/UX & Display Systems Design Rules

## 1. Dual Display Architecture & Address Mapping
- **Bus Assignment:** Dedicated `I2C_NUM_1` bus (400 kHz Fast Mode).
- **Physical Displays:** Two 0.96" SSD1306 OLED screens (128×64 resolution each).
  - **Left Screen (Master Overview):** I2C Address `0x3C` (Virtual coordinates $X: 0 \rightarrow 127, Y: 0 \rightarrow 63$).
  - **Right Screen (Detail & Settings):** I2C Address `0x3D` (Virtual coordinates $X: 128 \rightarrow 255, Y: 0 \rightarrow 63$).
- **Memory Structure:** 8 pages of 8 vertical pixels per panel. Total framebuffer size: 2048 bytes (2 KB RAM).

## 2. PCNT Quadrature Encoder Configuration
- **Hardware Module:** ESP32-S3 PCNT (Pulse Count Controller), Unit 0.
- **Glitch Filter:** Enabled with threshold of 1000 APB clock cycles (~12.5 $\mu\text{s}$) to eliminate contact bounce noise.
- **Pulse Limits:** Set high/low limits (e.g., $-100 \rightarrow +100$) with a quadrature step divider (2 or 4 pulses per physical detent click) to ensure 1 detent click = 1 menu navigation step.

## 3. Master-Detail Navigation Hierarchy
- **Master View (Left OLED):**
  - Displays summary rows for CH0, CH1, CH2, CH3 simultaneously.
  - Metrics shown: Selected Chemistry icon, Current State (IDLE/RUN/ERR), Live Voltage ($V$), Live Current ($A$).
- **Detail View (Right OLED):**
  - **`UI_STATE_CHANNEL_DETAIL`:** Deep telemetry for selected channel ($Ah$, $Wh$, Temperature, DCIR $m\Omega$, elapsed time).
  - **`UI_STATE_SETTINGS`:** Interactive menu for chemistry selection, target current adjustment, cutoff voltage, and thermal limits.
  - **`UI_STATE_ALARM_OVERLAY`:** High-priority Modal Overlay. Inverted black-on-white flashing box showing Fault Bitmask Name (e.g., `ERR_OVER_TEMP`) and prompt `[ CLICK TO ACK ]`.

## 4. Immediate Mode GUI (IMGUI) Pipeline
1. Clear Virtual Framebuffer (memset 0x00, 2048 bytes)
2. Fetch Active State from system_state_get_metrics()
3. Render Master Screen Elements (X: 0..127)
4. Render Active Detail/Menu Screen Elements (X: 128..255)
5. If metrics.error_flags != 0 -> Draw Inverted Overlay Box (X: 138..245, Y: 10..54)
6. Flush Left Panel (0x3C) -> Flush Right Panel (0x3D)