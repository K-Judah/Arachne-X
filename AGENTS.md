# Instructions for coding agents

## Scope and baseline

Arachne-X is an early-stage hexapod, not a working autonomous robot. Read README.md, docs/SOFTWARE_ARCHITECTURE.md, docs/DEVELOPMENT_PLAN.md and the relevant Documents/ engineering notes before changes. The 2026-10-02 audit is in the development plan. No executable software or build/test commands existed at that audit.

The current hardware baseline is six legs, three joints per leg (coxa, femur, tibia), 18 MG996R servos, Raspberry Pi 4B, an ESP32-WROOM-32U based development board, two PCA9685 controllers, and camera-based vision. Exact board variant, camera model, wiring and mechanical geometry are TBD. Old DS3218, ESP32 DevKit V1 and single-PCA9685 references are historical, not implementation inputs.

Explicit user instructions take precedence. For software planning, use the current baseline and docs/ architecture; preserve historical engineering requirements and flag conflicts rather than silently changing physical designs. Architectural proposals are not evidence of implemented capabilities.

## Preserve engineering work

- Do not edit, delete, rename, regenerate or reformat CAD binaries (.ipt, .iam, .dwg, .stl), assemblies, or image/video assets during software tasks. Only an explicit request to change those assets can expand that scope.
- Keep CAD/, Assembly/, Images/ and Videos/ intact. Never assume an assembly is complete or resolve missing dependencies by removing files.
- Do not remove duplicate specifications or empty placeholders as incidental cleanup. Consolidation needs its own scoped change preserving unique information and links.
- Inspect git status before work. Preserve unrelated user changes; stage explicit paths and review the staged diff before committing.

## Software boundaries

- ESP32 owns motion timing, IK, gait execution, joint limits, calibration, PCA9685 output and local fault handling. Raspberry Pi owns operator arbitration, mission intent, vision and dashboard services.
- Keep portable motion/control logic independent of ESP32 and I2C APIs; use injectable clock, transport and servo interfaces for host tests.
- Use stable named joints and configuration for channel assignments. Do not embed mechanical dimensions, pulse endpoints, pin assignments or mirrored joint signs in motion code.
- Mark unknown physical values TBD in documentation and unresolved/null in future machine-readable configuration. Reject incomplete configuration before arming; never convert TBD to zero or guessed defaults.
- Keep geometry, wiring and per-servo calibration separately versioned and tied to the physical robot revision. Use explicit units and coordinate frames.
- Start disarmed. Require validated configuration, matching protocol/configuration versions and explicit arm requests. A reconnect or reboot must not resume motion automatically.
- Treat MG996R motion as open-loop unless actual feedback hardware is added. Commanded joint positions are not measured positions. Missing sensor values are unavailable, not zero or healthy.
- Never energize servos, flash connected hardware, or start live movement as a side effect of builds/tests. Hardware operation requires explicit scope and a documented bench procedure.

## Validation and delivery

Use small, independently reviewable implementation milestones from docs/DEVELOPMENT_PLAN.md. Do not implement the entire robot in a scaffolding or documentation task. When adding software, document and pin its toolchain, add meaningful offline tests and CI, and provide actual reproducible build/test commands. Do not claim proposed commands were run.

For documentation changes, check links, consistency, git diff --check and the full staged diff. For code, test the affected contracts, limits and failure behavior as well as the normal path. Record unavailable hardware validation honestly. Verify protected asset hashes or an equivalent before/after comparison, summarize changed files and remaining TBDs, and commit only the requested scope when authorized.
