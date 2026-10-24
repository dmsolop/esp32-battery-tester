# KiCad PCB Routing & Layout Design Rules

## 1. Net Classes & Track Parameters
Configure net classes in KiCad via **Board Setup → Design Rules → Net Classes**:
- **Power (GND, GND_PWR, +12V, +3.3V, VDD):** Track width 0.4 mm – 0.6 mm (16–25 mil).
- **Signal (Standard digital/analog):** Track width 0.15 mm – 0.2 mm (6–8 mil).
- **RF (ESP32 antenna line):** Track width ≥ 0.5 mm, outer layer only, continuous width, zero vias, 135° or curved arc bends.
- **ESP32-S3 Specific Power Rails:**
  - Main +3.3V supply lines: ≥ 0.635 mm (25 mil).
  - VDD3P3 on Pin 2/3: ≥ 0.5 mm (20 mil).

## 2. Noise Reduction & Analog/Digital Domain Isolation
- **Physical Zone Separation:** Keep analog components (ADS1115 ADC, LM358 op-amps, DS18B20 sensors, voltage dividers) physically separated from digital components (ESP32-S3 MCU, logic ICs) by at least 5–10 mm, or isolate using GND guard traces.
- **Orthogonal Layer Routing (Crosstalk Reduction):** Route traces on adjacent layers perpendicularly (e.g., TOP layer horizontal, BOTTOM layer vertical) to reduce capacitive/inductive coupling and crosstalk by 3x–5x. For critical analog lines, place a continuous GND plane on the inner layer as a shield.
- **Trace Length Minimization:** Keep analog traces as short as possible (< 300 mm, ideally < 30 mm for ADC inputs).

## 3. General Trace Geometry & Loop Area Rules
- **Rule of "3W":** Maintain center-to-center trace spacing ≥ 3× trace width. Maintain at least 1.0 mm separation between high-speed digital clocks (ESP32 240 MHz CLK) and analog sensor lines.
- **Corner Bends:** Strictly avoid 90° sharp corners. Use 135° bends or smooth arcs to prevent impedance discontinuities.
- **Parallel Trace Avoidance:** Avoid parallel routing of signal lines over long distances. If parallel routing is unavoidable, place a GND guard trace with via stitching every 5–10 mm.
- **Loop Area Minimization:** Place GND return vias within < 3 mm of signal layer-change vias. Keep ESP32 3.3V power loop areas minimal to contain 500 mA Wi-Fi current spikes.

## 4. Ground Planes & Net Tie Topology
- **Star Ground (`NetTie_2`):** Separate dirty power ground (`GND_PWR` for MOSFETs, shunts, fan drivers) and clean analog ground (`GND` for ADS1115, LM358, ESP32) using a dedicated `NetTie_2` (`NetTie:NetTie-2_SMD_c0.5mm`). Place the Net Tie at the single physical reference point at the shunt resistor array.
- **Polygon Usage:**
  - 2-Layer Boards: Use copper fills (polygons) on signal layers.
  - 4+ Layer Boards: Dedicated internal solid planes for GND and power rails.
- **Stitching & Thermal Vias:** Via stitching around RF traces and solid ground fills under power stages. Place thermal vias under MOSFETs/radiators.

## 5. Power Supply Decoupling & ADC Filtering
- **Bulk Capacitors (10 µF – 100 µF):** Tantalum capacitors (SMD Case A/B, low ESR/ESL, high thermal stability) or ceramic capacitors near power entry (1–3 cm from regulator). Aluminum electrolytic capacitors are strictly forbidden.
- **Bypass Capacitors (100 nF – 1 µF):** Ceramic X7R/X5R capacitors placed < 5 mm from IC power pins. Connect directly to ground plane using via stitching adjacent to pads (not directly on pads to avoid solder wicking).
- **Advanced Filtering for Critical ADC / VREF Nodes:**
  - **LC / Ferrite Bead Filtering:** Ferrite bead (e.g., 100 Ω @ 100 MHz) + 1–10 µF capacitor for VREF / AVDD.
  - **π-Filters (C-L-C):** Configured for precision ADC power supply lines.
  - **RC Input Filters:** RC filter (R = 100 Ω – 1 kΩ, C = 10 nF – 100 nF) in front of ADC inputs (e.g., 190k/10k divider with 100nF filter).