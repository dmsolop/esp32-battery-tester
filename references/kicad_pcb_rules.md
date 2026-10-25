# KiCad PCB Routing & Layout Design Rules

## 1. European Standard Notation & Footprints
- **Notation:** Always use European standard values: `uF`, `mOhm`, `nH`, `4k7`, `0R033`.
- **SMD Footprints:** Enforce `_HandSolder` or `_Handsoldering` suffix for passive SMDs to allow manual prototyping. 
- **Capacitors:** Bulk decoupling MUST use Tantalum (Case A/B) or Ceramic. Aluminum electrolytic capacitors are STRICTLY FORBIDDEN due to high ESR and volume.

## 2. Power & Signal Track Widths
Configure net classes in KiCad via **Board Setup $\rightarrow$ Design Rules $\rightarrow$ Net Classes**:
- **Power (GND, GND_PWR, +12V, +3.3V, VDD):** Track width 0.4 mm – 0.6 mm (16–25 mil).
- **Signal (Standard digital/analog):** Track width 0.15 mm – 0.2 mm (6–8 mil).
- **RF (ESP32 Antenna):** Track width $\ge 0.5$ mm, outer layer only, continuous width, zero vias, 135° or curved arc bends.
- **ESP32-S3 Specific Power Rails:**
  - Main +3.3V supply lines: $\ge 0.635$ mm (25 mil).
  - VDD3P3 on Pin 2/3: $\ge 0.5$ mm (20 mil).

## 3. High-Current Power Stage (140W "Sandwich")
- **MOSFET Routing (IRFP250N):** Ensure massive, unmasked copper polygons for Drain and Source to handle up to 15A per transistor.
- **Thermal Management:** Place dense thermal via arrays (0.3mm drill, 0.6mm pad) under MOSFET pads and directly beneath the heatsink contact zones. 
- **Zener & Snubber Placement:** The 15V Gate-Source Zener (BZT52C15) and RC snubber (10 Ohm + 100nF) MUST be placed as physically close to the MOSFET pins as possible to minimize parasitic inductance.

## 4. 4-Wire Kelvin Sensing & Net Tie Topology
- **Current Shunt Array:** The 3x 33mOhm 2512 SMD array must be routed symmetrically to ensure equal current distribution. 
- **Kelvin Connections:** Route thin `LOAD_SENSE` traces from the inner edge of the 2512 pads. High-current `LOAD_FORCE` traces must connect to the outer edge of the pads.
- **Star Ground (`NetTie_2`):** Maintain strict separation between heavy power discharge (`GND_PWR`) and precision sensing (`GND`). Connect them at a single reference point at the shunt resistor array using `NetTie:NetTie-2_SMD_c0.5mm`.

## 5. Noise Reduction & Analog/Digital Isolation
- **Physical Zone Separation:** Keep analog components (ADS1115 ADC, LM358 op-amps, DS18B20 sensors) physically separated from digital logic (ESP32-S3) by at least 5–10 mm, or isolate using GND guard traces.
- **Orthogonal Layer Routing:** Route traces on adjacent layers perpendicularly (e.g., TOP layer horizontal, BOTTOM layer vertical) to reduce capacitive/inductive crosstalk by 3x–5x.
- **Trace Length Minimization:** Keep analog traces as short as possible (< 30 mm for ADC inputs).

## 6. General Trace Geometry & Loop Area Rules
- **Rule of "3W":** Maintain center-to-center trace spacing $\ge 3\times$ trace width. Maintain at least 1.0 mm separation between high-speed digital clocks (ESP32 CLK) and analog sensor lines.
- **Corner Bends:** Strictly avoid 90° sharp corners. Use 135° bends or smooth arcs to prevent impedance discontinuities.
- **Parallel Trace Avoidance:** Avoid parallel routing of signal lines over long distances. If unavoidable, place a GND guard trace with via stitching every 5–10 mm.
- **Loop Area Minimization:** Place GND return vias within < 3 mm of signal layer-change vias. Keep ESP32 3.3V power loop areas minimal to contain 500 mA Wi-Fi current spikes.

## 7. Power Supply Decoupling & ADC Filtering
- **Bypass Capacitors (100 nF – 1 µF):** Ceramic X7R/X5R capacitors placed < 5 mm from IC power pins. Connect directly to ground plane using via stitching adjacent to pads (not directly on pads).
- **Advanced Filtering for Critical ADC / VREF Nodes:**
  - VREF / AVDD: Ferrite beиad (100 $\Omega$ @ 100MHz) + 1-10µF capacitor.
  - ADC Inputs: RC filter (100 $\Omega$ - 1k$\Omega$, 10nF - 100nF) immediately before ADC pins.