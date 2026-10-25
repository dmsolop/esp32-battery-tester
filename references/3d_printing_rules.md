# 3D Printing & Mechanical Design Rules (Target: Bambu Lab P1S)

## 1. Physical Constraints & Layer Geometry
- **Max Print Volume:** 250 × 250 × 250 mm. Split larger enclosures using interlocking joints (dovetail or lap joints) with a 0.15 mm clearance.
- **Wall Thickness (0.4mm Nozzle):** Design walls as exact multiples of 0.42 mm (extrusion width). Minimum structural wall: 1.68 mm (4 perimeters). Heavy load bearing: 2.52 mm (6 perimeters).
- **Layer Heights:** 
  - 0.20 mm for standard structural parts (standoffs, internal ducts).
  - 0.16 mm for external fascia, OLED bezels, button caps, and fine tolerances.
- **Overhangs & Chamfers:** Avoid supports where possible. Limit overhangs to 45°. Use 45° chamfers (0.8–1.0 mm) on all bottom edges to eliminate "Elephant's foot" and ensure perfect mating of split parts.

## 2. Material Selection & Thermal Zones
- **ABS / ASA (Hot Zone - Slaves):** MANDATORY for power module enclosures, fan ducts, and "Sandwich" heatsink mounts. Operational limit: 95°C. Design with uniform wall thickness and use mouse-ear brims on sharp corners to prevent warping on the P1S.
- **PETG (Cold Zone - Master):** Used for the main chassis, UI panels, OLED bezels, and non-heated PCB standoffs. Operational limit: 75°C.
- **PLA is STRICTLY FORBIDDEN:** Do not use PLA for any component due to low thermal deflection temperature (55°C).

## 3. Tolerances, Fits & Fasteners
- **Clearance Gaps:**
  - **Tight Press-Fit (Connectors):** 0.10 mm clearance. Use for XT60, USB-C, and RJ45 panel mounts to withstand mechanical insertion forces.
  - **Standard Assembly Fit:** 0.15 mm – 0.20 mm (sliding covers, modular rail joints).
  - **Loose Fit (Kinematic):** 0.25 mm – 0.30 mm (rotary encoder knobs, tactile buttons).
- **Heat-Set Brass Inserts:** Never thread screws directly into plastic. Use M2/M3/M4 brass inserts. 
  - **Design Rule:** The printed hole diameter must be 0.2 mm smaller than the insert's outer diameter (e.g., M3 OD 4.2 mm = 4.0 mm hole). Add a 0.5 mm deep chamfer at the hole opening for precise thermal centering.

## 4. Part Orientation & Structural Integrity
- **Z-Axis Weakness:** Never orient parts so that mechanical stress (like pulling an XT60 connector or tightening a heatsink clamp) pulls layers apart. Print load-bearing clips and brackets flat on the X/Y plane.
- **PCB Standoffs:** Minimum standoff diameter is 6.0 mm with a 4.0 mm hole for M3 inserts. Keep standoffs under 15 mm tall to prevent snapping; if taller, add ribbing or 45° gussets to the base.

## 5. Aesthetics & Thermal Ducting
- **Fascia Printing:** Design the front UI panels (OLEDs, encoder) to be printed face-down on the Bambu Textured PEI Plate for a professional matte finish.
- **"Sandwich" Ducting:** Air ducts must securely wrap the dual Intel Celeron heatsinks. Air intake vents must be ≥ 120% of the fan's cross-sectional area. Exhaust vents must route hot air completely out of the chassis, strictly isolating it from the ESP32-S3 Master board and ADCs.