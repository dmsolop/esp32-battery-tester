# User Inventory & Component Stock Specification

This document serves as the Single Source of Truth (SSOT) for all physical components available in the user's inventory.
**Rule for AI Agents:** Always prioritize using these specific components for schematics, breadboard prototyping, and PCB design before proposing new purchases.

---

## 1. Microcontrollers, ICs & Active Semiconductors

| Component Name          | Type / Description                          | Package / Case | Quantity | Parameters / Notes                                               |
| :---------------------- | :------------------------------------------ | :------------- | :------- | :--------------------------------------------------------------- |
| ESP32-S3-WROOM-1U-N16R8 | MCU Module (Wi-Fi/BT)                       | Module         | 1 шт     | 16MB Flash, 8MB PSRAM, External antenna connector only           |
| ESP32-S3-WROOM-1U-N16R8 | MCU Module (Wi-Fi/BT)                       | Module         | 1 шт     | 16MB Flash, 8MB PSRAM, PCB antenna only                          |
| ESP32-S3 N16R8          | MCU Module (Wi-Fi/BT)                       | Module         | 1 шт     | 16MB Flash, 8MB PSRAM, PCB antenna + External connector          |
| ADS1115IDGSR            | 16-Bit ADC with PGA                         | MSOP-10        | 5 шт     | I2C interface, BOGI silkscreen mark                              |
| LM358DT                 | Dual Operational Amplifier                  | SOIC-8 (SMD)   | 8 шт     | General purpose Op-Amp                                           |
| LM358N                  | Dual Operational Amplifier                  | DIP-8 (THT)    | 20 шт    | Dual Op-Amp for shunts/filters                                   |
| УД608                   | Precision Op-Amp                            | DIP-8 (THT)    | 18 шт    | Low input bias current (uA741 analogue)                          |
| TL431                   | Adjustable Precision Shunt Regulator        | TO-92 (THT)    | 5 шт     | 2.5V - 36V reference voltage                                     |
| AMS1117-3.3             | LDO Voltage Regulator                       | SOT-223        | 4 шт     | 3.3V Output, 1A max                                              |
| L7812CV                 | Linear Voltage Regulator                    | TO-220         | 1 шт     | +12V Output, 1.5A max with heatsink                              |
| PC817                   | Optocoupler / Optoisolator                  | DIP-4 (THT)    | 2 шт     | Phototransistor output                                           |
| CR5229                  | Offline PWM Controller                      | DIP-8 (THT)    | 1 шт     | Built-in high voltage power switch                               |
| UC3843B                 | Current Mode PWM Controller                 | DIP-8 (THT)    | 2 шт     | Low UVLO threshold (8.4V turn-on)                                |
| UC3842N                 | Current Mode PWM Controller                 | DIP-8 (THT)    | 1 шт     | High UVLO threshold (16V turn-on)                                |
| OB2538AP                | High Performance Offline PWM Controller     | DIP-8 (THT)    | 2 шт     | Energy-efficient charger/adapter driver                          |
| NE555P                  | Precision Timer IC                          | DIP-8 (THT)    | 2 шт     | Pulse generation, timing, PWM                                    |
| KIA494P                 | Dual PWM Control IC                         | DIP-16 (THT)   | 1 шт     | TL494 equivalent for push-pull converters                        |
| TOP245YN                | Integrated Offline Switcher                 | TO-220-7C      | 1 шт     | TOPSwitch-GX family, high power PWM                              |
| DS18B20 (UMW)           | 1-Wire Digital Temperature Sensor           | TO-92 (THT)    | 3 шт     | -55°C to +125°C, 9-12 bit resolution                             |
| DS18B20                 | 1-Wire Digital Temperature Sensor           | TO-92 (THT)    | 5 шт     | -55°C to +125°C, 9-12 bit resolution                             |
| DS18B20                 | 1-Wire Digital Temp Sensor (Waterproof)     | External Probe | 3 шт     | Sealed cable probe assembly                                      |
| ME1A-0512               | Isolated DC-DC Converter                    | SIP            | 1 шт     | 1W 5V to 12V DC-DC converter (Favotek)                           |

---

## 2. Transistors, Diodes, Triacs & Power Switches

| Component Name     | Type / Category                | V_max  | I_max    | Package  | Quantity | Description / Parameters                                   |
| :----------------- | :----------------------------- | :----- | :------- | :------- | :------- | :--------------------------------------------------------- |
| IRFP150NPBF        | N-Channel Power MOSFET         | 100V   | 42A      | TO-247   | 8 шт     | Power Discharge Stage Transistor                           |
| IRLZ44N            | N-Ch Logic-Level MOSFET        | 55V    | 47A      | TO-220   | 4 шт     | Logic-Level Gate Drive (V_GS = 5V full open)               |
| HRF3205            | N-Channel Power MOSFET         | 55V    | 110A     | TO-220   | 3 шт     | Ultra-low R_DS(on) Power Switch                            |
| IRF3205PBF         | N-Channel Power MOSFET         | 55V    | 110A     | TO-220   | 10 шт    | Lead-Free high current power switch                        |
| IRFZ44EPBF         | N-Channel Power MOSFET         | 60V    | 48A      | TO-220   | 8 шт     | Standard N-Ch MOSFET                                       |
| P4NK60ZPF          | High-Voltage N-Ch MOSFET       | 600V   | 4A       | TO-220F  | 2 шт     | Zener-protected gate                                       |
| IRLML2502TRPBF-JSM | N-Ch Logic-Level MOSFET        | 20V    | 4.2A     | SOT-23   | 12 шт    | Fan Control / Auxiliary Load Driver                        |
| 2N2222A            | NPN Bipolar Transistor         | 40V    | 0.8A     | TO-92    | 4 шт     | General purpose switching                                  |
| S9013              | NPN Bipolar Transistor         | 25V    | 0.5A     | TO-92    | 1 шт     | Low noise AF amplifier / switch                            |
| S9015              | PNP Bipolar Transistor         | 45V    | 0.1A     | TO-92    | 1 шт     | Low noise AF amplifier                                     |
| BC547              | NPN Bipolar Transistor         | 45V    | 0.1A     | TO-92    | 4 шт     | Small signal switching                                     |
| BC557B             | PNP Bipolar Transistor         | 45V    | 0.1A     | TO-92    | 4 шт     | Small signal switching (Complement to BC547)               |
| 2SA733P            | PNP Bipolar Transistor         | 50V    | 0.15A    | TO-92    | 1 шт     | Small signal PNP                                           |
| 2SC945             | NPN Bipolar Transistor         | 50V    | 0.15A    | TO-92    | 5 шт     | Small signal NPN                                           |
| BD139              | NPN Power Transistor           | 80V    | 1.5A     | TO-126   | 1 шт     | Medium power driver                                        |
| 13003              | NPN High-Voltage BJT           | 400V   | 1.5A     | TO-126   | 10 шт    | High-voltage fast switching                                |
| 13002S             | NPN High-Voltage BJT           | 400V   | 1.2A     | TO-92    | 2 шт     | High-voltage switcher                                      |
| 13005A             | NPN High-Voltage BJT           | 400V   | 4.0A     | TO-220   | 7 шт     | High-voltage medium power switch                           |
| 2SC4242            | NPN High-Voltage BJT           | 400V   | 7.0A     | TO-220F  | 2 шт     | High-power switching transistor                            |
| BT131-600          | Sensitive Gate Triac           | 600V   | 1.0A     | TO-92    | 9 шт     | AC Load control directly from MCU                          |
| BTA12-600          | Isolated Power Triac           | 600V   | 12A      | TO-220   | 4 шт     | Insulated tab AC power control                             |
| BTA12-800          | Isolated Power Triac           | 800V   | 12A      | TO-220   | 2 шт     | Insulated tab AC power control                             |
| BTA212X-800E       | Logic-Level Power Triac        | 800V   | 12A      | TO-220F  | 1 шт     | Fully insulated, sensitive gate (I_GT < 10mA)              |
| MBRF20100CT        | Dual Schottky Diode            | 100V   | 20A      | TO-220F  | 1 шт     | Common Cathode (10A per diode)                             |
| PSR10C40CT         | Dual Schottky Diode            | 40V    | 10A      | TO-220   | 1 шт     | Common Cathode (5A per diode)                              |
| PR3002             | Fast Recovery Diode            | 100V   | 3.0A     | DO-201AD | 2 шт     | Fast switching rectifier                                   |
| PR1002             | Fast Recovery Diode            | 100V   | 1.0A     | DO-41    | 8 шт     | Fast switching rectifier                                   |
| T,ST,1N 4148       | High Speed Signal Diode        | 100V   | 0.2A     | DO-35    | 21 шт    | THT Switching diode                                        |
| 1N4148W            | High Speed Signal Diode        | 100V   | 0.15A    | SOD-123  | 50 шт    | SMD Switching diode                                        |
| BAT54S KL4         | Dual Schottky Diode            | 30V    | 0.2A     | SOT-23   | 100 шт   | Series configuration for clamping protection               |
| 1N5817             | Schottky Rectifier             | 20V    | 1.0A     | DO-41    | 3 шт     | Low forward voltage drop                                   |
| 1N5822             | Schottky Rectifier             | 40V    | 3.0A     | DO-201AD | 10 шт    | Low forward voltage drop                                   |
| 1N5408             | Standard Rectifier             | 1000V  | 3.0A     | DO-201AD | 10 шт    | Power mains rectifier                                      |
| 1N4007             | Standard Rectifier             | 1000V  | 1.0A     | DO-41    | 38 шт    | General purpose rectifier                                  |
| SR8200             | Schottky Rectifier             | 200V   | 8.0A     | DO-201AD | 8 шт     | High current Schottky                                      |
| 2CL4512            | High Voltage Rectifier         | 12000V | 0.5A     | HV Tube  | 1 шт     | 12kV High voltage diode                                    |
| BZT52C15 WJ        | Zener Diode                    | 15V    | 500mW    | SOD-123  | 100 шт   | Gate-Source Voltage Protection                             |
| SMBJ3V3-E3/52      | TVS Diode (Suppressor)         | 3.3V   | 600W     | SMB      | 4 шт     | Uni-directional ESD / Transient protection                 |
| SMCJ58A            | TVS Diode (Suppressor)         | 58V    | 1500W    | SMC      | 4 шт     | Uni-directional Overvoltage Protection                     |
| USBLC6-2SC6        | Very Low Capacitance ESD TVS   | 5.25V  | I_PP 5A  | SOT-23-6 | 2 шт     | USB 2.0 High Speed Data Line Protection                    |

---

## 3. Resistors, Shunts & Potentiometers

| Component Value | Type / Technology            | Package / Footprint | Rating / Tolerance    | Qty    | Breadboard Fit | Notes                                    |
| :-------------- | :--------------------------- | :------------------ | :-------------------- | :----- | :------------- | :--------------------------------------- |
| 2512 5mR (R005) | Manganese-Copper Alloy Shunt | 2512 (SMD)          | 3W, 0.5%              | 10 шт  | No             | Precision Low-Inductance Current Shunt   |
| 0.62 Ohm        | Wirewound Precision Shunt    | Ceramic THT         | 10W, ±1%              | 1 шт   | Yes            | High Power Current Sense Shunt           |
| 0.27 Ohm        | Wirewound Precision Shunt    | Ceramic THT         | 10W, ±1%              | 1 шт   | Yes            | High Power Current Sense Shunt           |
| 0.2 Ohm         | Carbon Film                  | Axial THT           | 0.25W, ±5%            | 4 шт   | Yes            | Low-value current sensing                |
| 0.47 Ohm        | Cement Power Resistor        | Ceramic Box THT     | 5W, ±5%               | 1 шт   | Yes            | Power discharge load                     |
| 0.51 Ohm        | Fusible Resistor             | Axial THT           | 2W, ±5%               | 4 шт   | Yes            | Protective current limiting              |
| 1.0 Ohm         | Wirewound                    | Ceramic THT         | 10W, ±5%              | 4 шт   | Yes            | High Power load resistor                 |
| 1.0 Ohm         | Metal Film                   | Axial THT           | 0.5W, ±5%             | 4 шт   | Yes            | Small current limiting                   |
| 2.0 Ohm         | Carbon Film                  | Axial THT           | 0.25W, ±5%            | 1 шт   | No             | Lead trimmed                             |
| 2.2 Ohm         | Cement Power Resistor        | Ceramic Box THT     | 3W, ±10%              | 1 шт   | Yes            | Power load                               |
| 2.2 Ohm         | Carbon Film                  | Axial THT           | 0.25W, ±5%            | 1 шт   | No             | Lead trimmed                             |
| 4.7 Ohm         | Metal Film                   | Axial THT           | 0.25W, ±5%            | 18 шт  | Yes            | Low resistance switch                    |
| 5.1 Ohm         | Metal Film                   | Axial THT           | 0.5W, ±20%            | 2 шт   | Yes            | Low power load                           |
| 10 Ohm          | Metal Film                   | 0805 (SMD)          | 0.125W, 150V, ±1%     | 100 шт | No             | RC Snubber / Gate resistor               |
| 10 Ohm          | Metal Film                   | Axial THT           | 1W, ±5%               | 9 шт   | Yes            | Gate drive / Power resistor              |
| 10 Ohm          | Fusible Resistor             | Axial THT           | 1W, ±5%               | 5 шт   | Yes            | Fuse protection                          |
| 10 Ohm          | Carbon Film                  | Axial THT           | 0.5W, ±5%             | 10 шт  | Yes            | Standard pull resistor                   |
| 20 Ohm          | Carbon Film                  | Axial THT           | 0.25W, ±5%            | 18 шт  | Yes            | Gate damping resistor                    |
| 22 Ohm          | Metal Film                   | Axial THT           | 0.25W, ±5%            | 18 шт  | Yes            | Damping resistor                         |
| 39 Ohm          | Carbon Film                  | Axial THT           | 0.25W, ±5%            | 2 шт   | No             | Lead trimmed                             |
| 51 Ohm          | Carbon Film                  | Axial THT           | 0.125W, ±5%           | 2 шт   | Yes            | Small signal                             |
| 56 Ohm          | Carbon Film                  | Axial THT           | 2W, ±5%               | 1 шт   | Yes            | Power discharge                          |
| 100 Ohm         | Metal Film                   | 0805 (SMD)          | 0.125W, 150V, ±1%     | 100 шт | No             | Low-pass RC filter                       |
| 100 Ohm         | Metal Film                   | Axial THT           | 0.25W, ±1%            | 20 шт  | Yes            | Precision filter resistor                |
| 100 Ohm         | Carbon Film                  | Axial THT           | 0.25W, ±5%            | 10 шт  | Yes            | Filter resistor                          |
| 150 Ohm         | Carbon Film                  | Axial THT           | 0.25W, ±5%            | 20 шт  | Yes            | LED / Current limit                      |
| 220 Ohm         | Carbon Film                  | Axial THT           | 0.5W, ±5%             | 10 шт  | Yes            | Pull-up / LED                            |
| 330 Ohm         | Carbon Film                  | Axial THT           | 0.25W, ±5%            | 2 шт   | No             | Lead trimmed                             |
| 750 Ohm         | Carbon Film                  | Axial THT           | 0.25W, ±5%            | 7 шт   | Yes            | Pull resistor                            |
| 1.0 kOhm        | Metal Film                   | 0805 (SMD)          | 0.125W, 150V, ±1%     | 100 шт | No             | Divider / Logic pull                     |
| 1.0 kOhm        | Metal Film                   | Axial THT           | 0.25W, ±1%            | 20 шт  | Yes            | Precision pull-up                        |
| 1.0 kOhm        | Carbon Film                  | Axial THT           | 0.5W, ±5%             | 10 шт  | Yes            | Medium power pull                        |
| 1.0 kOhm        | Carbon Film                  | Axial THT           | 0.25W, ±5%            | 1 шт   | No             | Lead trimmed                             |
| 1.5 kOhm        | Carbon Film                  | Axial THT           | 0.25W, ±5%            | 1 шт   | No             | Lead trimmed                             |
| 1.6 kOhm        | Metal Film                   | Axial THT           | 2W, ±5%               | 1 шт   | Yes            | Power bleeder                            |
| 2.2 kOhm        | Metal Film                   | 0805 (SMD)          | 0.125W, 150V, ±1%     | 100 шт | No             | I2C Pull-up                              |
| 2.2 kOhm        | Carbon Film                  | Axial THT           | 0.25W, ±5%            | 1 шт   | No             | Lead trimmed                             |
| 2.7 kOhm        | Carbon Film                  | Axial THT           | 0.25W, ±5%            | 3 шт   | No             | Lead trimmed                             |
| 3.9 kOhm        | Carbon Film                  | Axial THT           | 0.25W, ±5%            | 2 шт   | No             | Lead trimmed                             |
| 4.7 kOhm        | Metal Film                   | 0805 (SMD)          | 0.125W, 150V, ±1%     | 100 шт | No             | I2C Pull-up (100kHz / 400kHz bus)        |
| 4.7 kOhm        | Metal Film                   | Axial THT           | 0.25W, ±1%            | 20 шт  | Yes            | 1-Wire DS18B20 Pull-Up Resistor          |
| 4.7 kOhm        | Carbon Film                  | Axial THT           | 0.25W, ±5%            | 5 шт   | No             | Lead trimmed                             |
| 5.1 kOhm        | Metal Film                   | 0805 (SMD)          | 0.125W, 150V, ±1%     | 100 шт | No             | Precision divider                        |
| 9.1 kOhm        | Carbon Film                  | Axial THT           | 0.25W, ±5%            | 1 шт   | No             | Lead trimmed                             |
| 10 kOhm         | Metal Film (ARNA0805)        | 0805 (SMD)          | Precision Thin Film   | 4 шт   | No             | High stability telemetry divider         |
| 10 kOhm         | Metal Film (ERA6ARB)         | 0805 (SMD)          | Precision Thin Film   | 4 шт   | No             | High stability telemetry divider         |
| 10 kOhm         | Metal Film                   | 0805 (SMD)          | 0.125W, 150V, ±1%     | 100 шт | No             | General logic pull-up / ADC divider      |
| 10 kOhm         | Metal Film                   | Axial THT           | 0.25W, ±1%            | 20 шт  | Yes            | Precision pull-up                        |
| 10 kOhm         | Carbon Film                  | Axial THT           | 0.5W, ±5%             | 10 шт  | Yes            | Pull-up resistor                         |
| 10 kOhm         | Carbon Film                  | Axial THT           | 0.25W, ±5%            | 2 шт   | No             | Lead trimmed                             |
| 18 kOhm         | Carbon Film                  | Axial THT           | 0.25W, ±5%            | 3 шт   | No             | Lead trimmed                             |
| 33 kOhm         | Carbon Film                  | Axial THT           | 0.25W, ±5%            | 1 шт   | No             | Lead trimmed                             |
| 68 kOhm         | Carbon Film                  | Axial THT           | 0.25W, ±5%            | 1 шт   | Yes            | High impedance                           |
| 82 kOhm         | Carbon Film                  | Axial THT           | 0.25W, ±5%            | 1 шт   | No             | Lead trimmed                             |
| 100 kOhm        | Carbon Film                  | Axial THT           | 0.5W, ±5%             | 10 шт  | Yes            | High impedance pull                      |
| 150 kOhm        | Carbon Film                  | Axial THT           | 0.25W, ±5%            | 2 шт   | No             | Lead trimmed                             |
| 205 kOhm        | Metal Film                   | Axial THT           | 0.25W, ±1%            | 20 шт  | Yes            | High precision telemetry divider         |
| 330 kOhm        | Carbon Film                  | Axial THT           | 0.25W, ±5%            | 21 шт  | Yes            | High impedance divider                   |
| 470 kOhm        | Carbon Film                  | Axial THT           | 0.25W, ±5%            | 1 шт   | No             | Lead trimmed                             |
| 510 kOhm        | Carbon Film                  | Axial THT           | 0.25W, ±5%            | 13 шт  | Yes            | Divider resistor                         |
| 510 kOhm        | Metal Film                   | Axial THT           | 0.25W, ±5%            | 7 шт   | Yes            | Divider resistor                         |
| 560 kOhm        | Carbon Film                  | Axial THT           | 0.25W, ±5%            | 19 шт  | Yes            | High value bias                          |
| 680 kOhm        | Carbon Film                  | Axial THT           | 0.25W, ±5%            | 21 шт  | Yes            | High value bias                          |
| B10K            | Potentiometer                | Panel Mount THT     | Variable 10 kOhm      | 1 шт   | Yes            | Rotary angle feedback                    |
| 128 T W 682     | Trimmer Resistor             | PCB Mount THT       | Multi-turn 6.66k      | 1 шт   | Yes            | Zero-offset hardware tuning              |
| 50 kOhm         | Variable Resistor            | PCB Mount THT       | 0.25W, ±20%           | 1 шт   | Yes            | Adjustable resistance                    |

---

## 4. Capacitors

| Value    | Voltage | Dielectric / Type        | Package / Footprint | Qty    | Notes                                                  |
| :------- | :------ | :----------------------- | :------------------ | :----- | :----------------------------------------------------- |
| 50 pF    | 3kV     | Ceramic Disc             | THT (5mm pitch)     | 1 шт   | High Voltage Disc (Marking: 47 3KV)                    |
| 220 pF   | 50V     | Multilayer Ceramic (MLCC)| THT Radial          | 1 шт   | Droplet style (Marking: 221)                           |
| 2200 pF  | 1kV     | Ceramic Disc             | THT (7mm Dia)       | 3 шт   | High Voltage Disc (Marking: 222 1KV)                   |
| 1 nF     | 50V     | Ceramic X7R 10%          | 0805 (SMD)          | 30 шт  | High-frequency bypass                                  |
| 1 nF     | 100V    | Film (CBB/MKT) ±5%       | THT Radial          | 1 шт   | Marking: .001J 100HT                                   |
| 1 nF     | 1kV     | Film (CBB/MKT) ±5%       | THT Radial          | 1 шт   | Marking: HY3A2 102J                                    |
| 1.2 nF   | 1kV     | Ceramic Disc             | THT Radial          | 1 шт   | High Voltage Disc (Marking: 102 1KV)                   |
| 3.3 nF   | 630V    | Film (CBB/MKT) ±10%      | THT Radial          | 9 шт   | High voltage snubber / filter                          |
| 4.8 nF   | 1kV     | Ceramic Disc             | THT Radial          | 1 шт   | High Voltage Disc (Marking: 472 1KV)                   |
| 6.8 nF   | 630V    | Film (CBB/MKT) ±10%      | THT Radial          | 19 шт  | Pulse / Snubber capacitor                              |
| 6.8 nF   | 2kV     | Film (CBB/MKT) ±5%       | THT Radial          | 6 шт   | High voltage pulse capacitor                           |
| 10 nF    | 50V     | Ceramic X7R 10%          | 0805 (SMD)          | 20 шт  | Telemetry filtering                                    |
| 10 nF    | 100V    | Ceramic Disc ±20%        | THT (5mm Dia)       | 7 шт   | Marking: 103 / 103Z                                    |
| 10 nF    | 630V    | Film (CBB/MKT) ±10%      | THT Radial          | 1 шт   | Snubber filter                                         |
| 47 nF    | 250V    | Film (CBB/MKT) ±5%       | THT Radial          | 1 шт   | Mains filter                                           |
| 47 nF    | 630V    | Film (CBB/MKT) ±10%      | THT Radial          | 9 шт   | High voltage filter                                    |
| 100 nF   | 50V     | Ceramic X7R 10%          | 0805 (SMD)          | 30 шт  | Standard IC Bypass Capacitor                           |
| 100 nF   | 100V    | Ceramic X7R 10%          | 0805 (SMD)          | 20 шт  | High-Voltage Line Decoupling                           |
| 100 nF   | 50V     | Multilayer Ceramic (MLCC)| THT Radial          | 6 шт   | Droplet style bypass (Marking: 104)                    |
| 100 nF   | 50V     | Film (CBB/MKT) ±5%       | THT Radial          | 9 шт   | Low noise audio/telemetry filter                       |
| 100 nF   | 100V    | Film (CBB/MKT) ±5%       | THT Radial          | 1 шт   | Film bypass                                            |
| 100 nF   | 250V    | Film (CBB/MKT) ±5%       | THT Radial          | 14 шт  | AC Mains suppression                                   |
| 100 nF   | 400V    | Film (CBB/MKT) ±5%       | THT Radial          | 1 шт   | AC Mains suppression                                   |
| 150 nF   | 50V     | Ceramic (KM-5 USSR) ±20% | THT Radial          | 2 шт   | Marking: 5F M15 WD                                     |
| 1 uF     | 50V     | Ceramic Y5V 20%          | 0805 (SMD)          | 13 шт  | Power line bypass                                      |
| 1 uF     | 50V     | Ceramic X7R 10%          | THT (5mm Pitch)     | 6 шт   | Radial THT Ceramic                                     |
| 1.5 uF   | 400V    | Electrolytic             | THT Radial          | 1 шт   | High voltage bulk cap                                  |
| 2.2 uF   | 50V     | Electrolytic             | THT Radial          | 1 шт   | Analog rail buffer                                     |
| 3.3 uF   | 400V    | Electrolytic             | THT Radial          | 11 шт  | High voltage offline power buffer                      |
| 4.7 uF   | 50V     | Electrolytic             | THT Radial          | 4 шт   | General decoupling                                     |
| 4.7 uF   | 400V    | Electrolytic             | THT Radial          | 6 шт   | High voltage offline power buffer                      |
| 10 uF    | 16V     | Tantalum low-ESR (106C)  | Case A 1206 (SMD)   | 10 шт  | Primary Bulk Decoupling (Mandatory, No Electrolytics)  |
| 10 uF    | 25V     | Electrolytic             | THT Radial          | 1 шт   | Standard decoupling                                    |
| 10 uF    | 35V     | Electrolytic             | THT Radial          | 10 шт  | Power rail buffer                                      |
| 10 uF    | 50V     | Electrolytic             | THT Radial          | 1 шт   | Power rail buffer                                      |
| 10 uF    | 450V    | Electrolytic             | THT Radial          | 6 шт   | Offline switching supply primary cap                   |
| 22 uF    | 16V     | Tantalum low-ESR (TAJ)   | Case B 3528 (SMD)   | 4 шт   | Primary Bulk Decoupling (Mandatory, No Electrolytics)  |
| 22 uF    | 16V     | Electrolytic SMD         | SIZE C (SMD)        | 9 шт   | Surface mount aluminum electrolytic                    |
| 47 uF    | 16V     | Electrolytic SMD         | SIZE D (SMD)        | 2 шт   | Surface mount aluminum electrolytic                    |
| 47 uF    | 35V     | Electrolytic             | THT Radial          | 8 шт   | Power supply output filter                             |
| 47 uF    | 50V     | Electrolytic             | THT Radial          | 5 шт   | Power supply filter                                    |
| 47 uF    | 400V    | Electrolytic             | THT Radial          | 5 шт   | Primary switching capacitor                            |
| 100 uF   | 16V     | Electrolytic SMD         | SIZE D (SMD)        | 3 шт   | Surface mount aluminum electrolytic                    |
| 100 uF   | 25V     | Electrolytic SMD         | SIZE E (SMD)        | 1 шт   | Surface mount aluminum electrolytic                    |
| 100 uF   | 16V     | Electrolytic             | THT Radial          | 6 шт   | Low voltage bulk buffer                                |
| 100 uF   | 25V     | Electrolytic             | THT Radial          | 2 шт   | Low voltage bulk buffer                                |
| 100 uF   | 35V     | Electrolytic             | THT Radial          | 8 шт   | General power rail reservoir                           |
| 100 uF   | 50V     | Electrolytic             | THT Radial          | 1 шт   | General power rail reservoir                           |
| 220 uF   | 16V     | Electrolytic SMD         | SIZE E (SMD)        | 2 шт   | Surface mount aluminum electrolytic                    |
| 220 uF   | 35V     | Electrolytic             | THT Radial          | 6 шт   | Output bulk capacitor                                  |
| 470 uF   | 25V     | Electrolytic             | THT Radial          | 2 шт   | Low ESR output smoothing                               |
| 470 uF   | 35V     | Electrolytic             | THT Radial          | 2 шт   | Low ESR output smoothing                               |
| 1000 uF  | 25V     | Electrolytic             | THT Radial          | 1 шт   | Large capacity reservoir                               |
| 1000 uF  | 35V     | Electrolytic             | THT Radial          | 1 шт   | Large capacity reservoir                               |

---

## 5. Inductors & EMI Suppression Filters

| Component Name | Value / Inductance | Current Rating | Package / Case | Qty  | Description / Notes                                     |
| :------------- | :----------------- | :------------- | :------------- | :--- | :------------------------------------------------------ |
| EMI-фільтр     | Ferrite Bead       | Unrated        | Axial THT      | 8 шт | Unmarked ferrite bead for high-frequency noise damping  |
| 1000 мкГн      | 1000 uH (1.0mH)    | 0.1A (100mA)   | Axial THT      | 1 шт | Choke inductor (turquoise body 4.1x8mm, code: Br-Bl-R-S)|

---

## 6. Connectors, Interfaces & Mechanical Contacts

| Interface Name | Type / Specifications                      | Mounting / Style    | Qty   | Application / Usage                            |
| :------------- | :----------------------------------------- | :------------------ | :---- | :--------------------------------------------- |
| RJ45 Socket    | 8P8C Female Cable Socket (No lamp)         | Panel / PCB Header  | 10 шт | Multi-channel remote telemetry / sensor header |
| USB-C Socket   | USB Type-C 16-Pin Female Jack              | SMD / SMT Solder    | 10 шт | Main Power Entry & Firmware Flashing           |
| XT60PT-M       | XT60 Male Connector                        | PCB Through Hole    | 4 шт  | High-Current Discharge Power Inlets            |
| MF3-4F         | Micro-Fit Connector Socket                 | Cable / PCB Header  | 5 шт  | Power Stage Channel interconnect               |
| MF3-4MRA       | Micro-Fit Connector Header Right Angle     | PCB Mount           | 5 шт  | Power Stage Channel interconnect               |
| MX-43645-0200  | Micro-Fit Housing                          | Cable Mount         | 5 шт  | Channel wiring harness                         |
| XM1-3.00-1x02-R| Micro-Fit Connector 2-Pin                  | PCB Mount           | 5 шт  | Fan power / auxiliary header                   |

---

## 7. Fuses & Circuit Protection Elements

| Part Number        | Rating / Specs     | Package / Footprint | Qty   | Reset Type                          |
| :----------------- | :----------------- | :------------------ | :---- | :---------------------------------- |
| 1812 PPTC Fuse     | 100mA, 60V         | 1812 / 4532 (SMD)   | 20 шт | Self-Recovery Resettable Fuse       |
| 1206 PPTC Fuse     | 500mA, 13.2V       | 1206 / 3216 (SMD)   | 20 шт | Self-Recovery Resettable Fuse       |
