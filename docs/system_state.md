# System State: ESP32-S3 Smart Modular Battery Analyzer

**Last Updated:** November 2026
**Current Phase:** Engineering Prototyping and Breadboarding (blocking PCB routing).

---

## 1. Current Status
- Laboratory prototyping and bench testing of the power stage and digital control interface (Stages 1–3) have been **successfully completed and validated** on physical hardware.
- According to the Asymmetric Blocking Rule, **PCB routing and final layout execution for `slave_board` and `battery_analyzer_hw` are officially UNBLOCKED**.
- Developing hierarchical schematics and moving toward final PCB routing (`slave_board` and `battery_analyzer_hw`).
- Approved global pinout for the ESP32-S3-WROOM-1 MCU, accounting for hardware constraints (Boot, Strapping, USB) and separation of high-frequency and analog signals.
- Fully implemented firmware control algorithms: PID controller, PWM DAC path (Variant B2), dual ADS1115 current sensing shunts, fan PWM cooling profile, interactive dual SSD1306 UI with viewport scrolling, and 1-Wire DS18B20 role EEPROM binding mechanism.

## 2. Changelog & Agreements
- **Architectural audit of PID controller, PWM LPF (Variant B2), and transition to single 2512 R005 shunts (November 2026):**
- **Elimination of double integration in PID:** Identified a mathematical collision in `components/load_control/pid_service.c` (combining the positional integral `ctx->integral += error * dt_sec` with incremental output accumulation `ctx->current_output += output`, which created an $I^2$ link and self-oscillations). Approved transition to a pure positional PID form according to `references/pid_math_rules.md` with dynamic Anti-Windup.
- **Approval of passive LPF with divider (Variant B2) and 20 kHz PWM:** To generate the `V_REF` reference voltage, a dual-stage passive RC filter on fast `100 nF` capacitors with a built-in resistive divider was approved (`R1 = 10 kOhm` for breadboard with `0.27 Ohm` or `33 kOhm` for `5 mOhm`, `R_div = 1.0 kOhm || C1 = 100 nF`, `R2 = 4.7..10 kOhm`, `C2 = 100 nF`) at a PWM frequency of `19.53 kHz` (`20 kHz`, 12-bit, `PWM_MAX_DUTY = 4095`). This ensures ripple `< 0.59 mV (pk-pk)`, settling time `1.5..3.0 ms`, and a perfect zero at `PWM = 0` without the second Op-Amp zero-offset error.
- **Transition to single Manganin 2512 R005 shunts (5 mOhm, 3W):** Instead of a parallel array of three 33 mOhm resistors (11 mOhm) per MOSFET leg, the use of one precision Copper-Manganese `2512 R005` shunt (`0.005 Ohm`, `3W`, `0.5%`) per transistor was approved (`R5` for `Q1` and `R8` for `Q2`, removing `R6, R7, R9, R10`). Power dissipation at 15 A per leg drops to `1.125 W` (`>60%` margin), simplifying the Kelvin topology. For the dual-leg board, summing the currents of both ADC channels (`AIN0 + AIN1`) is mandatory.
- **Full Laboratory Validation of Breadboard Prototype (Stages 1–3) & Firmware Modernization (November 2026):**
  - **Stage 1 (Vref static, shunt current, MCU hardware Reset):** Validated Variant B2 dual-stage passive LPF with 10:1 divider on pin 3 of LM358 op-amp (`R1=10k`, `R_div=1.0k || C1=100nF`, `R2=10k`, `C2=100nF`). Ripple pk-pk is `< 0.5 mV`. Verified clean, non-oscillating load current at 300 mA ($V_{shunt} = 80\text{ mV}$, $V_{GS} = 2.19\text{ V}$) and 1000 mA ($V_{shunt} = 290\text{ mV}$, $V_{GS} = 2.56\text{ V}$) on ceramic $0.27\ \Omega$ test shunt. Upon MCU hardware Reset pull-down, load current drops to exactly `0.00 A` without gate spikes.
  - **Stage 2 (DCIR 100 ms pulse mode & HW Self-Test):** Rising edge settling time measured on oscilloscope is $t_{90\%} \approx 2.5\text{ ms}$ (strict criterion $< 3\text{ ms}$ achieved). Tare/zero calibration (-462 uA offset), hardware self-test pulse (~100.4 mA), and open circuit fault detection (`ERR_OPEN_CIRCUIT`) with power off operate reliably.
  - **Stage 3 (Full peripheral integration under ~1 A load):** Dual SSD1306 OLED panels (`0x3C`, `0x3D`) on Fast Mode I2C (400 kHz) and PCNT quadrature rotary encoder operate without I2C stalls or glitch counts under load. All 3 DS18B20 1-Wire sensors on `IO8` are detected and respond to thermal gradients; emergency shutdown on thermal runaway (`ERR_THERMAL_RUNAWAY`, Mask: 0x08) is verified.
  - **PCB Layout Unblock:** Hardware validation confirmed circuit stability; per Asymmetric Blocking Rule, PCB layout is unblocked.
  - **Interactive 1-Wire Sensor Role Binding via UI (`UI_STATE_SENSOR_BIND`):** Implemented sub-screen accessed from `[ TESTS & SAFE ]` -> `1W:H%d/C%d [ROLES]`. Lists sensors #0..#5 with live temperatures and ROM IDs, allowing one-click cycling through `NONE` -> `HS` -> `CELL` roles with direct commit to DS18B20 EEPROM (`T_L` byte: `0x01` for Heatsink, `0x02` for Cell) via `temp_service_bind_sensor_role()` and live bus rescan via `temp_service_rescan()`.
- **Pinout unification for compatibility with ESP32-S3-DevKitC-1 (N16R8) and protection against WPU upon reset (November 2026):**
- Unified pinout (Variant B) approved and synchronized between the `mcu_core.kicad_sch` schematic and firmware config (`Kconfig.projbuild`, `sdkconfig`):
- **Power Load PWM (`PWM_CH0..3`):** moved `PWM_CH1` to `IO1`, `PWM_CH3` to `IO2` (along with `IO38` for CH0 and `IO40` for CH2). All 4 pins have a `Hi-Z` state upon MCU reset, completely eliminating parasitic partial opening of power MOSFETs due to internal weak pull-ups (`Internal Weak Pull-Up`) on `IO39` and `IO41`.
- **Cooling FAN PWM (`FAN_CH0..3`):** assigned to `IO12` (CH0), `IO13` (CH1), `IO14` (CH2), `IO21` (CH3). Freed `IO48` for full hardware compatibility with the built-in WS2812 LED on `ESP32-S3-DevKitC-1` boards.
- **Audio Indication (Buzzer):** assigned `BUZZER_PIN` to `IO42` with a mandatory external `4.7 kOhm` pull-down resistor to `GND` to reliably close the transistor switch during MCU boot/reset.
- Completely freed pins `IO39`, `IO41`, `IO48`.
- **Master Board Netlist Verification:** The updated `battery_analyzer_hw.net` netlist is fully verified. All circuits of the U3 microcontroller (ESP32-S3-WROOM-1) comply with Variant B unified pinout: `PWM_CH0..3` (`IO38`, `IO1`, `IO40`, `IO2`), `FAN_CH0..3` (`IO12`, `IO13`, `IO14`, `IO21`), `BUZZER_PIN` (`IO42` with R36 4.7k pull-down to GND), `IO39`/`IO41`/`IO48` (`NC`), `I2C_PWR` (`IO4`/`IO5`), `I2C_UI` (`IO6`/`IO7`), `1W_CH0..3` (`IO8..11`), input devices (`IO15..18`), `ADC_ALERT` (`IO47`), `USB_D-/D+` (`IO19`/`IO20`).
- **I2C Synchronization and Netlist Verification (November 2026):**
- Coordinated and synchronized I2C bus hardware pinout in the `battery_analyzer_hw.net` netlist with the ESP-IDF firmware:
- `I2C_PWR` (power telemetry bus for Slave modules): `SDA` = `IO4`, `SCL` = `IO5`.
- `I2C_UI` (SSD1306 displays): `SDA` = `IO6`, `SCL` = `IO7`.
- Conducted full verification of `battery_analyzer_hw.net` and `slave_board.net` netlists before starting breadboard assembly:
- Confirmed integrity of symmetrical power lines: `LOAD_FORCE_+` (XT60 pin 1 -> F1 -> Drain Q1/Q2 -> C3/C4 -> TVS D1 SMCJ58A Cathode) and `GND_PWR` (`LOAD_FORCE_-`, XT60 pin 2 -> shunts -> TVS D1 Anode -> NetTie NT1 to signal `GND`).
- Confirmed differential Kelvin connection: `LOAD_SENSE_+` (Micro-Fit J_SENSE1 pin 1) -> R15 (190k) -> AIN2 (U2 ADS1115), `LOAD_SENSE_-` (Micro-Fit J_SENSE1 pin 2) -> R16 (10k) + C9 (100nF) -> AIN3 (U2 ADS1115).
- Confirmed current measurement connection: `I_SENSE_Q1` -> AIN0, `I_SENSE_Q2` -> AIN1.
- **Prototyping Priority:** Any PCB routing work is prohibited until successful validation of current stability on the breadboard.
- **Audio Indication (Buzzer):** Approved addition of a piezo transducer for alarms (OVP, OCP, OTP) and UI feedback. Controlled via transistor switch.
- **ADS1115 (PGA) Configuration:** Dynamic scaling split: voltage channel = +/- 4.096V (bits 11-9 = 001), current channel = +/- 0.256V (bits 11-9 = 101).
- **Local vs Global Labels:** Approved use of local labels for intra-sheet connections to preserve global namespace cleanliness.
- `V_REF` remains local on the `/Power_Stage/` sheet.
- `FAN_GND` remains local on the `/cooling_thermal/` sheet.
- **PWM_IN:** Confirmed that the `PWM_IN` label on the `/Power_Stage/` sheet is already global and correctly receives the signal from the RJ45 connector (J1).
- **EMC and Testing Conditions:** Factoring in operation in panel buildings (reinforced concrete walls). Routing requires strict shielding. SOONDA S8602 is used for interference checks.

## 3. Hardware Baseline
- **MCU:** ESP32-S3-WROOM-1 / compatible with ESP32-S3-DevKitC-1 (N16R8).
- **Power Stage:** MOSFET IRLZ44N (or IRFP150N/IRFP250N), IRLML2502TRPBF-JSM, 1N4148W SOD-323 diodes, single precision Manganin 2512 R005 shunts (0.005 Ohm / 5 mOhm, 3W, 0.5%) one per MOSFET (during breadboard prototyping — ceramic 0.27 Ohm / 270 mOhm).
- **ADC & Interrupts:** ADS1115 (I2C_PWR_SDA: `IO4`, I2C_PWR_SCL: `IO5`, ADC_ALERT: `IO47`).
- **Load PWM:** `IO38` (CH0), `IO1` (CH1), `IO40` (CH2), `IO2` (CH3) — all Hi-Z on reset, frequency `19.53 kHz` (`20 kHz`, 12-bit, Variant B2 passive dual-stage LPF with divider and `100 nF` capacitors).
- **Cooling FAN PWM:** `IO12` (CH0), `IO13` (CH1), `IO14` (CH2), `IO21` (CH3) (Cooling: Intel A65061-002).
- **Temperature Sensors (1-Wire):** `IO8`, `IO9`, `IO10`, `IO11`.
- **UI (RTC-domain):** I2C_UI (I2C_UI_SDA: `IO6`, I2C_UI_SCL: `IO7`), Encoder (`IO15`, `IO16`, `IO17`), Tare Button (`IO18`).
- **Indication:** Piezo transducer (Buzzer) via transistor switch on `IO42` (with external `4.7 kOhm` pull-down to `GND`).
- **Free / Reserved Pins:** `IO39`, `IO41`, `IO48` (RGB LED WS2812 on DevKit).

## 4. Active Handoff / Queue

### 🔬 Hardware Lab Engineer (Status: Completed - Interactive Bench Validation of [ TESTS & SAFE ] and [ ROLES ])
- [x] **BLOCKER:** Assemble on breadboard the Variant B2 dual-stage passive LPF with 10:1 divider (`R1=10k`, `R_div=1.0k || C1=100nF`, `R2=4.7k..10k`, `C2=100nF`) and check current stability at 20 kHz PWM frequency. Successfully validated on FNIRSI DPOS350P: 2.5 ms rise time, minimal ripple, stable current of 300 mA (V_shunt 80 mV, V_gate 2.19 V) and 1000 mA (V_shunt 290 mV, V_gate 2.56 V), on Reset current drops to 0.00 A without spikes.
- [x] Conduct current stability testing using Bench Power Supply and oscilloscope, verify absence of high-frequency oscillations, and ensure a clean zero at `PWM = 0` and MCU reset.
- [x] **Audit Stage 3 (1-Wire Peripherals, FAN IO12, I2C/PCNT):** Hardware and software disconnects identified, Cross-Module Impact Tickets generated for Orchestrator.
- [x] **Step-by-Step Bench Validation of `[ TESTS & SAFE ]` and `1W:H%d/C%d [ROLES]` UI Menu:**
  - `[ HW Self-Test ]`: **PASS**. Transitions FSM to `STATE_SELF_TEST`, verifies power path and switches to Live Monitor.
  - `[ Pre-Check OCV ]`: **FAIL / BLOCKING FSM BUG**. Successfully measures OCV, but FSM unconditionally transitions to `STATE_DISCHARGING` pulling 950 mA instead of remaining in diagnostic/IDLE mode. Furthermore, Stop button in Live Monitor fails to abort discharge until `ERR_THERMAL_RUNAWAY` (0x08) triggers.
  - `[ Zero Calib REL ]`: **PARTIAL / UX-BUG**. Correctly records raw ADC zero offset into driver, but lacks visual confirmation on OLED / logs. Intermittent CRC error detected on 1-Wire bus (`ROM: 28C1...`).
  - `RawPWM: %4u`: **PARTIAL / BUG**. Hardware PWM opens MOSFET and scales discharge current smoothly, but PCNT input jumps erratically (e.g. from 2600 to 250/150) due to 16-bit encoder counter overflow/glitch.
  - `FanPWM: %3u%%`: **PASS**. Smooth fan control 0..100% in 10% steps, immediate acoustic and airflow response.
  - `Sag/DCIR: %4umO`: **FAIL / UX-BUG**. Stale value (177 mOhm) persists across restarts; needs metric reset on new test start and an interactive single-pulse DCIR trigger action.
  - `NiMH Pk: %4lumV`: **FAIL / ARCH-DEFECT**. Static read-only field without clear diagnostic purpose in manual testing menu; needs relocation or replacement with interactive feature.
  - `1W:H%d/C%d [ROLES]` (`render_sensor_bind`): **PASS**. Full live list #0..#2 with ROM and temperatures, seamless cyclic role toggle (`NONE` -> `HS` -> `CELL`), real-time commit to DS18B20 EEPROM, successful `[ Rescan 1-Wire ]`, and correct counter update in parent menu.
  - `[ <- BACK ]`: **PASS**. Safely zeros active `RawPWM` to 0 mA and resets cooling fan to automatic thermal curve.
- [ ] Develop test plan for internal resistance and voltage sag on cylindrical Li-ion and NiMH cells.

### 💻 Firmware Engineers (`load-control-engineer` & `hal-driver-specialist`) (Status: In Progress - AGILE)
- [x] **[Ticket-PID-1]** In `components/load_control/pwm_driver.c` and `include/pwm_driver.h`, switch PWM to `19531 Hz` (`20 kHz`) frequency and `LEDC_TIMER_12_BIT` resolution (`PWM_MAX_DUTY = 4095`).
- [x] **[Ticket-PID-2]** In `components/load_control/pid_service.c`, eliminate double integration (`ctx->current_output += output`) by implementing a pure positional PID form with dynamic Anti-Windup per `references/pid_math_rules.md`.
- [x] **[Ticket-ADC-1]** In `components/adc_driver/adc_driver.c` and `adc_driver.h`, resolved precision loss during integer division (implemented formula `(raw_adc * 15625ULL) / (2ULL * CONFIG_SHUNT_RESISTOR_MOHM)` in 64-bit space) and added support for polling both current channels `AIN0` (`0xCB83`, Q1) and `AIN1` (`0xDB83`, Q2) while maintaining backward compatibility for current 0.27 Ohm breadboard.
- [x] **[Cross-Ticket-CTRL-1]** Implemented CLI command `raw_pwm <ch> <duty>` in `components/cli/cli.c` with direct call to `hw_set_load_pwm()` for Vref testing and MOSFET opening bypassing state machines.
- [x] **[Cross-Ticket-CTRL-2]** Adapted timings and `STATE_SELF_TEST` threshold in `components/load_control/load_control.c` for Variant B2 LPF (cyclic polling up to 200 ms, 10 mA detection threshold).
- [x] **[Ticket-CTRL-3]** Resolved test PWM overwrite defect: added `manual_override` flag and `load_control_set_raw_pwm(ch, duty)` function. The background `pid_control_task` does not reset PWM in `STATE_IDLE`/`STATE_ERROR` states if manual mode is active, and safely clears `manual_override` upon fault or transition to standard test cycles.
- [x] **[Ticket-UI-1]** Implemented interactive 2-stage parameter editing mode in the `UI_STATE_SETTINGS` submenu (`components/ui_interface/ui_interface.c`): selection of items 0..2 (current, voltage, temp) via encoder, enter/exit edit mode (`is_editing`) via encoder button click with visual `> <` indication, current step 50 mA (50..3000 mA), voltage step 50 mV (2500..4500 mV), temp step 1 °C (30..80 °C), synchronization to `system_state` and updating sensor limits.
- [x] **[Ticket-UI-SCROLL-STOP]** Implemented universal windowed scrolling engine (`update_scroll_offset`, `VISIBLE_ROWS = 4`, `^`/`v` scroll indicators at X=248) for `UI_STATE_CH_DETAIL` and `UI_STATE_SETTINGS` in `components/ui_interface/ui_interface.c`. Added dynamic `[ -> START ]` / `[ [] STOP ]` button with immediate discharge abort on click, chemistry switching via `system_state_set_chemistry()` ensuring NVS and safe ROM defaults persistence, and non-terminating exit from `UI_STATE_GRAPH` back to channel details.
- [x] **[Ticket-UI-MONITOR]** Created dedicated Live Telemetry & Monitoring page (`render_live_monitor()` replacing legacy stub in `UI_STATE_GRAPH`) in `components/ui_interface/ui_interface.c`. Implemented 7 telemetric rows (`LIVE_MONITOR_TOTAL_ITEMS = 7`) with windowed encoder scrolling (`VISIBLE_ROWS = 4`, scroll indicators `^`/`v` at X=248): V/I, DCIR (`m.internal_res_mohm`) & Power, dual heatsink temperatures (HS1/HS2 from bound 1-Wire sensors), Cell temperature with thermal limit `(M: %2dC)`, accumulated Capacity ($mAh$) & Energy ($mWh$), OCV & Peak voltage, elapsed active time and SOH verdict. Renamed menu item in `UI_STATE_CH_DETAIL` to `[ LIVE MONITOR ]`, non-blocking click returns to detail view without interrupting discharge.
- [x] **[Ticket-UI-TEST-MENU]** Extended `UI_STATE_SETTINGS` in `components/ui_interface/ui_interface.c` with Watchdog safety limits (`CapLim`, step 250 mAh, 500..20000 mAh; `TimeLim`, step 15 min, 15..1440 min) and `[ Reset Defaults ]` restoring safe ROM values. Added `UI_STATE_TEST_DIAG` state accessed via `[ TESTS & SAFE ]` in `UI_STATE_CH_DETAIL` featuring windowed scrolling (4 visible rows) for: launching `[ HW Self-Test ]` and `[ Pre-Check OCV ]` with direct jump to Live Monitor, `[ Zero Calib REL ]` offset calibration, interactive `RawPWM: %4u` live duty adjustment (0..4095, step 50 via `load_control_set_raw_pwm()`), and telemetry indicators for `Sag/DCIR`, `NiMH Pk` and bound 1-Wire sensors count. Added `load_control` and `adc_driver` component requirements to `components/ui_interface/CMakeLists.txt`.
- [x] **[Ticket-UI-1W-ROLES]** Implemented interactive 1-Wire sensor role binding submenu (`UI_STATE_SENSOR_BIND`) in `components/ui_interface/ui_interface.c`. Accessed via `[ TESTS & SAFE ]` -> `1W:H%d/C%d [ROLES]`. Shows live list of all sensor slots (#0..#5) with real-time temperature, ROM signature bytes, and current role (`HS`, `CELL`, `NONE`). Direct encoder click cycles through roles and permanently writes the configuration into DS18B20 scratchpad/EEPROM via `temp_service_bind_sensor_role()`. Added dedicated menu item `[ Rescan 1-Wire ]` calling `temp_service_rescan()` to refresh the OneWire bus topology without resetting the MCU.
- [x] **[Ticket-THERMAL-FAN-STEP4]** Complete implementation of Step 4 thermal and cooling subsystems:
  - Extended DS18B20 driver with `ds18b20_write_scratchpad()` and `ds18b20_copy_scratchpad()` in `components/temp_sensor/ds18b20.c` and `ds18b20.h`.
  - Implemented 3-sensor support per channel in `components/temp_sensor/temp_service.c` with two-phase binding: reading pre-programmed EEPROM roles (`0x01` = Heatsink, `0x02` = Cell), followed by automatic fallback for unprogrammed factory sensors (up to 2 `SENSOR_ROLE_HEATSINK` with 85 °C limit + 1 `SENSOR_ROLE_CELL` with dynamic `settings.thermal_limit_mc`).
  - Added programmatic role rebinding API `temp_service_bind_sensor_role()` persisting changes to sensor EEPROM and updating `system_state`.
  - Implemented LEDC cooling fan driver in `components/load_control/pwm_driver.c` on `LEDC_CHANNEL_4..7` tied to timer 0 (~19.53 kHz), with emergency stop ramping fans to 100%.
  - Added background automatic fan thermal control curve in `components/load_control/load_control.c` (< 40 °C: 0%, 40..75 °C: linear 30..100%, >= 75 °C: 100%) with manual override via `load_control_set_fan_pwm(ch, pct)`.
  - Added interactive `FanPWM: %3u%%` item (editable in 10% steps, 0..100%) and interactive 1-Wire test action `1W:H%d/C%d [SCAN]` into `UI_STATE_TEST_DIAG` in `components/ui_interface/ui_interface.c`.
  - Enriched telemetry JSON output in `components/telemetry/telemetry.c` with `t_hs1_mc`, `t_hs2_mc`, and `t_cell_mc`.
- [x] **[Ticket-UI-BUF-TRUNC]** Fixed `-Werror=format-truncation=` in [`components/ui_interface/ui_interface.c:541`](components/ui_interface/ui_interface.c:541) (`render_sensor_bind`) by expanding `title` buffer from 20 to 24 bytes, preventing compiler truncation warnings for `uint8_t` channel index string `- CH%d 1W SENSORS -`.

### 👑 Master System Orchestrator (Status: Pending)
- [ ] Delegate execution of `Ticket-PID-1`, `Ticket-PID-2` and `Ticket-ADC-1` to `code` mode.
- [ ] Create a non-blocking FreeRTOS task to generate buzzer audio patterns without delaying main control cycle.
- [ ] Prepare software architecture to handle global `ADC_ALERT` interrupt on pin `IO47`.
- [ ] Verify logic separating I2C buses (`I2C_UI` and `I2C_PWR`) at the firmware level.

### 🛠 KiCad Hardware Engineer (Status: Pending - Unblocked for schematic tasks)
- [ ] **[Ticket-HW-1]** In `hardware/slave_board/power_stage.kicad_sch`, replace shunt arrays `R5..R7` and `R8..R10` (3x 33 mOhm) with single `0.005` shunts (`2512 R005`, 5 mOhm, 3W) as `R5` and `R8`, deleting `R6, R7, R9, R10`.
- [ ] **[Ticket-HW-2]** In `hardware/slave_board/power_stage.kicad_sch`, update PWM LPF to Variant B2 (`C1 = 100nF`, `C2 = 100nF`, add divider resistor to GND parallel to `C1`).
- [ ] Continue routing 140W power modules (4-wire Kelvin sensing) **ONLY AFTER** successful test by Lab Engineer.
- [ ] Ensure `GND_PWR` and `FAN_GND` polygon topology does not create ground loops.
- [ ] Verify application of European notation and HandSolder rules configuration.

---
*Note for agents: Upon completion of your tasks, you are required to update this file, change execution status, and add new tasks or identified issues to the log.*