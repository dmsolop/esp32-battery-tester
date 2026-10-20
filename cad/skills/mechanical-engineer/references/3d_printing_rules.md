# 3D Printing & Mechanical Design Rules (Bambu Lab P1S Target)

## 1. Physical Constraints & Layer Geometry
- **Max Print Volume:** 250 × 250 × 250 mm for single-piece components. Enforce split/modular structures if dimensions exceed 250 mm.
- **Nozzle & Wall Line Width:** Standard 0.4 mm nozzle (0.42 mm line width). Wall thicknesses must be exact multiples of 0.4 mm (1.2 mm = 3 loops, 1.6 mm = 4 loops, 2.0 mm = 5 loops).
- **Layer Heights:** 0.20 mm for structural enclosures; 0.16 mm for fascia panels, bevels, and fine threads.
- **Overhangs & Chamfers:** Keep unsupported overhang angles < 45°. Use 45° chamfers (0.8–1.0 mm) on the bottom layer to eliminate "Elephant's foot" distortion.

## 2. Material Selection & Thermal Limits
- **ABS / ASA (High Temp Zones):** Primary material for power module enclosures, fan ducts, and heat sink mounting brackets. Operational limit up to 95°C.
- **PETG (Standard Enclosure):** Used for outer chassis parts, PCB standoffs, display bezels, and clips. Operational limit up to 75°C.
- **PLA Ban:** PLA is strictly forbidden for any structural or internal component due to low thermal deflection (55°C).

## 3. Tolerances, Fits & Fasteners
- **Tight Press-Fit:** 0.10 mm – 0.12 mm clearance gap.
- **Standard Assembly Fit:** 0.15 mm – 0.20 mm clearance gap for sliding covers, mounting slots, and board retention rails.
- **Loose Fit:** 0.25 mm – 0.30 mm for button caps and dynamic mechanical linkages.
- **Heat-Set Brass Inserts:** Never thread screws directly into plastic. Use brass heat-set inserts (M2/M3/M4). Holes must be sized specifically for thermal insertion (e.g., M3 insert OD 4.2 mm requires 4.0 mm 3D printed hole).

## 4. Ergonomics & Aesthetics
- **Front Panel Orientation:** Design fascia and display panels to be printed face-down on a Textured PEI Plate for a matte finish.
- **Airflow & Fan Ducts:** Ensure total intake vent area matches or exceeds 120% of the active fan cross-sectional area to minimize acoustic noise and static pressure resistance.
