# Documentation & Writing Style Rules

## 1. Professional Tone & Language
- **Language:** English is the mandatory language for all technical documentation, codebase comments, and Git commits.
- **Style:** Direct, concise, and engineering-focused. Avoid conversational fluff, ambiguous adjectives, and marketing jargon. Explain *why* and *how* at the physical and software level.

## 2. Structure for Component Documentation (`README.md` / `component.md`)
Each component documentation file must strictly contain:
- **Overview:** High-level summary of the component's purpose and its architectural context.
- **State & FSM Integration:** How this component interacts with the Single Source of Truth (`system_state`) and the global Finite State Machine.
- **Architecture & IPC:** Data flow, FreeRTOS task priority, mutex requirements, queue usage, and hardware ISR interactions.
- **API Reference:** Exact function signatures, parameters, return types, and thread-safety constraints.
- **Hardware & Pinout Mapping:** Exact GPIO assignments, I2C addresses, and hardware limits (must cross-reference `Kconfig.projbuild` and KiCad schematics).
- **Error Handling & Alarms:** Document how exceptions, timeouts, and hardware limits are handled, latched, and propagated to the UI.

## 3. Commit Message Standards (Conventional Commits)
- **Format:** `<type>(<scope>): <short description>` (e.g., `hw(power): add 15V zener to IRFP250N gates`).
- **Length:** Strictly under 20 words.
- **Types:** 
  - `feat` (new firmware feature)
  - `fix` (bug fix)
  - `docs` (documentation updates)
  - `refactor` (code restructuring)
  - `hw` (KiCad, PCB, or schematic changes)
  - `mech` (3D printing, CAD, or enclosure changes)
  - `test`, `chore`.

## 4. Truth & Synchronization Guidelines (Traceability)
- **No Theoretical Code:** Never document features that are purely theoretical as implemented; clearly distinguish between current active code and Roadmap/TODO items.
- **Hardware/Software Sync:** Ensure all pin numbers, ADC thresholds, and hardware constants are cited directly from `Kconfig.projbuild` or `docs/system_state.md`. 
- **Traceability Matrix:** If a hardware trace, KiCad netlist, or physical dimension changes, the documentation MUST reflect the update, and corresponding tickets for firmware/mechanical updates must be logged in `system_state.md`.