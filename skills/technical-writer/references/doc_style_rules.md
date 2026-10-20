# Documentation & Writing Style Rules

## 1. Structure for Component Documentation (`README.md` / `component.md`)
Each component documentation file must contain:
- **Overview:** High-level summary of the component's purpose and its placement in the architecture.
- **Architecture & IPC:** How it interacts with `system_state` (SSOT), FreeRTOS tasks, or hardware ISRs.
- **API Reference:** Exact function signatures, parameters, return types, and thread-safety constraints.
- **Hardware & Pinout Mapping:** Exact GPIO assignments, I2C addresses, and hardware limits (derived from Kconfig).
- **Changelog / Revision History:** Dated log of structural changes.

## 2. Commit Message Standards
- **Format:** `<type>(<scope>): <short description>`
- **Length:** Strictly under 15 words.
- **Language:** English.
- **Types:** `feat`, `fix`, `docs`, `refactor`, `hw`, `test`, `style`, `chore`.

## 3. Truth & Synchronization Guidelines
- Never document features that are purely theoretical as implemented; clearly distinguish between current code state and Roadmap TODO items.
- Ensure all pin numbers and hardware constants are cited from `Kconfig.projbuild` or `sdkconfig.h`.