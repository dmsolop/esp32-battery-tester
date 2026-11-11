# Project Specification: ESP32-S3 Smart Modular Battery Analyzer

**Product:** 4-Channel Autonomous Programmable Electronic Load & Battery Analyzer[cite: 4].
**Architecture:** ESP32-S3 Master-Slave Topology[cite: 4].

## 1. Hardware Architecture & Limits
- **Master-Slave Isolation:** The ESP32-S3 Master board is the digital "cold zone"[cite: 4]. High-current discharge occurs on isolated Slave modules[cite: 4].
- **Communication Bus:** High-frequency signals (I2C, 1-Wire, PWM) are routed via shielded RJ45 cables (FTP/STP)[cite: 4]. The cable shield is tied to analog GND, acting as a Faraday cage to block EMI[cite: 4].
- **Power Stage (Per Channel):** Maximum dissipation is >140W, up to 30A[cite: 4]. Uses dual IRFP150N MOSFETs (TO-247) in a "Sandwich" topology between two Intel Celeron heatsinks[cite: 4]. Hardware current balancing is enforced via dual independent CC-loops using an LM358 Op-Amp[cite: 4].
- **Precision Sensing (4-Wire Kelvin):** Current is measured across a single 5 mOhm (R005) 2512 SMD Manganese-Copper alloy precision shunt per MOSFET leg (3W rating, 1.125W dissipation at 15A)[cite: 4]. Kelvin connections are routed directly from the inner pads on the PCB copper[cite: 4].
- **ADC Scaling:** ADS1115 PGA dynamically scales to maintain 16-bit resolution across the 1V–60V range[cite: 4].
- **Hardware Bypass (Safety):** The `ADC_ALERT_PIN` triggers a hardware interrupt (GPIO 47) to instantly zero the PWM duty cycle via LEDC registers during over-current or over-voltage events[cite: 4].
- **Thermal Management & Fan Control:** Dedicated LEDC timers: `LEDC_TIMER_0` (19531 Hz, 12-bit) dedicated to 4 power load channels (`LEDC_CHANNEL_0..3`), and `LEDC_TIMER_1` (30 Hz, 12-bit) dedicated to 4 cooling fan channels (`LEDC_CHANNEL_4..7`) for 3-wire BLDC fans (Sanyo Denki San Ace 109X) switched via Low-Side N-MOSFETs with freewheeling clamp diodes.

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

## 6. NVS Security & Diagnostic Algorithms
- **NVS Wear Leveling:** Stores ONLY the selected chemistry type (`battery_chem_t`).
- **Volatile PRO Settings:** Custom high-risk settings (e.g., 30A limits) are kept strictly in RAM.
- **Safe Boot:** On reset, the FSM restores safe, hardcoded ROM defaults (e.g., 1A discharge, 60°C limit) based on the last saved chemistry.
- **NiMH/NiCd Termination:** Employs $-\Delta V$ drop detection (5-10mV) and dynamic $dT/dt$ thermal derivative analysis (>1°C/min) to prevent overcharging[cite: 4].
- **Voltage Sag Detection:** Calculates initial voltage drop in the first millisecond of discharge to reject degraded or fake cells (high internal resistance)[cite: 4].
- **1-Wire Auto-Routing:** The system dynamically assigns limits based on EEPROM roles (`0x01` = Heatsink hard limit 85°C, `0x02` = Cell dynamic limit)[cite: 4].

## 7. Metrological Principles & Competitive Advantages (User Manual & Whitepaper)

### 7.1 Physics of 4-Wire Kelvin Sensing in DC/Pulse Measurements (Why Probe Tare is Obsolete)
In classical 2-wire battery analyzers, the measured cell voltage $V_{measured}$ suffers from parasitic resistive drops across current-carrying cables, banana jacks, and crocodile clips:
$$V_{measured} = V_{cell} - I_{load} \cdot (2 R_{wire} + 2 R_{contact})$$
At high currents (e.g., $1\text{ A}$ to $30\text{ A}$), even low wire resistances ($2 \times 50\text{ m}\Omega = 100\text{ m}\Omega$) induce huge measurement errors ($\Delta V = 100\text{ mV}$ to $3000\text{ mV}$), which falsely triggers cutoff thresholds or corrupts DCIR calculations.

Our instrument employs a **true 4-wire Kelvin connection topology**:
1. **Force Pair ($F+, F-$):** Dedicated heavy-gauge copper leads carry the entire discharge current ($I_{load} \le 30\text{ A}$) to the MOSFET power stage and precision manganese-copper shunt ($5\text{ m}\Omega$).
2. **Sense Pair ($S+, S-$):** Independent, shielded sense lines connect directly to the battery terminal lugs and route to the ADS1115 differential instrumentation ADC inputs ($AIN2 - AIN3$) via a high-impedance voltage divider ($205\text{ k}\Omega / 10\text{ k}\Omega$, total input impedance $R_{in} \approx 215\text{ k}\Omega$).
3. **Kirchhoff's Current Law ($KCL$) Verification:** The leakage current $I_{sense}$ entering the measurement circuit is minuscule:
   $$I_{sense} = \frac{V_{cell}}{R_{in}} = \frac{4.2\text{ V}}{215\text{ k}\Omega} \approx 19.5\text{ }\mu\text{A}$$
   The parasitic voltage drop across the sense leads ($R_{sense} \approx 50\text{ m}\Omega$) is mathematically negligible:
   $$\Delta V_{error} = I_{sense} \cdot R_{sense} = 19.5\text{ }\mu\text{A} \cdot 0.05\text{ }\Omega \approx 0.976\text{ }\mu\text{V} < 1\text{ LSB}$$
   (Where $1\text{ LSB} = 125\text{ }\mu\text{V} \times 21.5 \approx 2.68\text{ mV}$).
4. **Complete Elimination of Wire Calibration:** In differential two-tier DCIR pulse evaluation ($I_1 \to I_2$):
   $$R_{DCIR} = \frac{V_1(I_1) - V_2(I_2)}{I_2 - I_1} = \frac{(V_{cell1} - I_{sense1} R_{sense}) - (V_{cell2} - I_{sense2} R_{sense})}{I_2 - I_1} \equiv \frac{\Delta V_{cell}}{\Delta I}$$
   Because the high-current path drop $\Delta V_{wire} = \Delta I_{load} \cdot R_{wire}$ never enters the sense loop, wire resistance cancels out completely. Manual probe zeroing (`Probe Tare` / `Nulling`) is **physically unnecessary** and thus set to $R_{wire} = 0\text{ m}\Omega$.

### 7.2 Superiority of Two-Step Pulse DCIR (IEC 61960: $100\text{ mA} \to 1000\text{ mA}$) over Consumer 1 kHz ACIR Milliohmmeters
Consumer battery impedance testers (e.g., YR1035+, RC3563) inject a high-frequency AC current ($1\text{ kHz}$ sine wave). While convenient for sorting brand new cells on a factory line, $1\text{ kHz}$ AC measurement is fundamentally misleading for evaluating real battery health under active load:

1. **Equivalent Circuit Physics (Randles Cell):**
   A chemical power cell consists of:
   - Bulk ohmic resistance $R_{ohm}$ (foils, electrolyte, tabs, welds);
   - Double-layer capacitance $C_{dl}$ (typically massive: $0.1\text{ F}$ to $>10\text{ F}$ across porous electrode interfaces);
   - Charge-transfer resistance $R_{ct}$ (Faradaic kinetics of intercalation/de-intercalation of ions through SEI layer);
   - Warburg impedance $Z_W$ (solid-state ion diffusion at low frequencies $<0.1\text{ Hz}$).
2. **AC Short-Circuit Effect of $1\text{ kHz}$:**
   The capacitive reactance of the electrochemical double layer at $1000\text{ Hz}$ is:
   $$X_{C_{dl}} = \frac{1}{2 \pi f C_{dl}} = \frac{1}{2 \pi \cdot 1000 \cdot 1.0\text{ F}} \approx 0.16\text{ m}\Omega \to 0$$
   Because $X_{C_{dl}} \approx 0$, the $1\text{ kHz}$ excitation signal bypasses $R_{ct}$ entirely and measures **only** the high-frequency ohmic electrolyte resistance:
   $$R_{ACIR}(1\text{ kHz}) \approx R_{ohm}$$
   Consequently, an aged, heavily cycled, or degraded battery with high charge-transfer resistance (crystallization, thickened SEI layer, active lithium loss) can show an "excellent" low $1\text{ kHz}$ ACIR reading, yet completely sag and shut down under a real DC load!
3. **The Analyzer's IEC 61960 Advantage ($R_{DCIR} = R_{ohm} + R_{ct}$):**
   Our modular analyzer implements standard-compliant two-tier pulsed DCIR ($100\text{ mA} \to 1000\text{ mA}$ with $450\text{ ms}$ steady-state plateau and 3-point tail averaging, as well as 10-cycle multi-pulse trimmed mean). 
   - A $450\text{ ms}$ pulse allows initial capacitive charging to settle while capturing Faradaic charge-transfer overpotentials:
     $$R_{DCIR} = R_{ohm} + R_{ct}$$
   - **Dynamic Degradation Profiling $R_{DCIR}(SoC)$:** Unlike static benchtop testers, our instrument logs up to 64 DCIR points throughout the actual discharge cycle ($100\% \to 0\%\text{ SoC}$), mapping the true battery internal resistance curve and predicting real thermal/voltage limits under actual operating loads.

