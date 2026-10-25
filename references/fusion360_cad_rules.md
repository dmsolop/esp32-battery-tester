# Fusion 360 Parametric CAD Design Rules

## 1. Component Hierarchy (Rule #1 of Fusion 360)
- **Never Design at Root:** Before creating any sketch or body, ALWAYS create a new Component. Activate the component before sketching.
- **Logical Grouping:** One physical part = One Component. Use sub-assemblies (nested components) for modular sections (e.g., `Enclosure_Bottom`, `Power_Module_Left`).

## 2. Parametric Design & User Parameters
- **No Hardcoded Magic Numbers:** Define all critical dimensions (e.g., `wall_thickness = 1.68`, `pcb_clearance = 0.2`, `m3_insert_hole = 4.0`) in the **Modify $\rightarrow$ Change Parameters** table.
- **Tolerance Variables:** Use variables for fits (e.g., `press_fit = 0.1`, `slip_fit = 0.2`) and apply them mathematically to sketch dimensions (e.g., `width + slip_fit`).

## 3. Sketch Discipline
- **Fully Constrained Sketches:** Every sketch MUST be fully constrained (all lines turn black).
- **Constraints Over Dimensions:** Use geometric constraints (Coincident, Equal, Horizontal/Vertical, Tangent) before adding numerical dimensions.
- **Simplicity:** Keep sketches simple. Do not draw fillets or chamfers in the sketch; apply them as 3D features later in the timeline.

## 4. Timeline & Feature Management
- **Timeline Hygiene:** Resolve yellow warnings and red errors immediately. Never leave a broken timeline.
- **Fillets/Chamfers Last:** Apply cosmetic and structural fillets/chamfers at the very end of the component's timeline to prevent breaking references during iterative changes.
- **Positioning:** Avoid the "Move" command for permanent placement. Use **Joints** (or **As-Built Joints** if designed in place) to define relationships between components.

## 5. Exporting for 3D Printing (Bambu Studio)
- Export components as `.step` files rather than `.stl` to maintain perfect mathematical curves and allow Bambu Studio to interpret arc geometry accurately.