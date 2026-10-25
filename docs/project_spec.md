# Project Specification: ESP32-S3 Smart Modular Battery Analyzer

**Product:** 4-Channel Autonomous Programmable Electronic Load & Battery Analyzer[cite: 4].
**Architecture:** ESP32-S3 Master-Slave Topology[cite: 4].

## 1. Hardware Architecture & Limits
- **Master-Slave Isolation:** The ESP32-S3 Master board is the digital "cold zone"[cite: 4]. High-current discharge occurs on isolated Slave modules[cite: 4].
- **Communication Bus:** High-frequency signals (I2C, 1-Wire, PWM) are routed via shielded RJ45 cables (FTP/STP)[cite: 4]. The cable shield is tied to analog GND, acting as a Faraday cage to block EMI[cite: 4].
- **Power Stage (Per Channel):** Maximum dissipation is >140W, up to 30A[cite: 4]. Uses dual IRFP250N MOSFETs (TO-247) in a "Sandwich" topology between two Intel Celeron heatsinks[cite: 4]. Hardware current balancing is enforced via dual independent CC-loops using an LM358 Op-Amp[cite: 4].
- **Precision Sensing (4-Wire Kelvin):** Current is measured across a 3x 33 mOhm 2512 SMD Metal Strip resistor array (11 mOhm equivalent, up to 6W dissipation)[cite: 4]. Kelvin connections are routed directly on the PCB copper[cite: 4].
- **ADC Scaling:** ADS1115 PGA dynamically scales to maintain 16-bit resolution across the 1V–60V range[cite: 4].
- **Hardware Bypass (Safety):** The `ADC_ALERT_PIN` triggers a hardware interrupt (GPIO 47) to instantly zero the PWM duty cycle via LEDC registers during over-current or over-voltage events[cite: 4].

## 2. Software Architecture & RTOS (FreeRTOS)
Strict Task Prioritization prevents network or UI latency from affecting control loops[cite: 4]:
1.  **Task_Safety (Highest Priority):** Event-driven via `ulTaskNotifyTake`[cite: 4]. Wakes instantly on hardware ISR to force `STATE_ERROR`[cite: 4].
2.  **Task_PID_Control (High Priority):** Hard real-time task (100 Hz) computing Time-Aware PID logic and integrating Ah/Wh capacity using 64-bit integer micro-math[cite: 4].
3.  **Task_UI (Medium Priority):** Passive observer task handling dual SSD1306 OLED rendering and PCNT encoder input[cite: 4].
4.  **Task_Telemetry (Low Priority):** Handles Wi-Fi and MQTT JSON packet transmission[cite: 4].

## 3. Finite State Machine (FSM)
The system is governed by a strict FSM. PWM generation is ONLY allowed in `STATE_DISCHARGING`[cite: 4].
- **STATE_INIT:** Loads safe default parameters from ROM; checks I2C/1-Wire buses[cite: 4].
- **STATE_IDLE:** PWM is 0[cite: 4]. UI navigation is active[cite: 4].
- **STATE_PRE_CHECK:** Measures Open Circuit Voltage (OCV)[cite: 4]. Delays for chemical relaxation before starting[cite: 4].
- **STATE_REL_CALIBRATION:** Records zero-offset for 2-wire cable resistance compensation[cite: 4].
- **STATE_DISCHARGING:** Active PID load regulation and capacity integration[cite: 4].
- **STATE_FINISHED:** Normal cutoff (e.g., minimum voltage reached)[cite: 4].
- **STATE_ERROR:** Hardware or software fault triggered[cite: 4]. Requires explicit user acknowledgement via encoder button to clear[cite: 4].

## 4. UI/UX & Display Engine
- **Dual OLED Pipeline:** Two SSD1306 displays (0x3C and 0x3D) rotated 180° via `esp_lcd_panel_mirror`, mapped to a single 256x64 virtual framebuffer[cite: 4].
- **Master-Detail Layout:** Left screen shows a 4-channel summary dashboard[cite: 4]. Right screen shows active channel details and settings[cite: 4].
- **Alarm Interception:** If `STATE_ERROR` occurs, the right screen halts standard rendering and flashes an inverted text banner (e.g., "VOLTAGE SAG") until acknowledged by the user[cite: 4].

## 5. Non-Volatile Storage (NVS) Security
- **Wear Leveling:** NVS `nvs_flash` stores ONLY the selected chemistry type (`battery_chem_t`) to minimize flash degradation[cite: 4].
- **Volatile PRO Settings:** Custom high-risk settings (e.g., 30A limits) are kept strictly in RAM[cite: 4].
- **Safe Boot:** On reset, the FSM restores safe, hardcoded ROM defaults (e.g., 1A discharge, 60°C limit) based on the last saved chemistry, preventing accidental high-current discharge into a new battery[cite: 4].

## 6. Diagnostic Algorithms
- **NiMH/NiCd Termination:** Employs $-\Delta V$ drop detection (5-10mV) and dynamic $dT/dt$ thermal derivative analysis (>1°C/min) to prevent overcharging[cite: 4].
- **Voltage Sag Detection:** Calculates initial voltage drop in the first millisecond of discharge to reject degraded or fake cells (high internal resistance)[cite: 4].
- **1-Wire Auto-Routing:** The system dynamically assigns limits based on EEPROM roles (`0x01` = Heatsink hard limit 85°C, `0x02` = Cell dynamic limit)[cite: 4].