# Mechanical Requirements

## Scope

This document defines the mechanical baseline for Arachne-X, a six-legged autonomous spider robot. It complements the project master specification and governs mechanical design, CAD, fabrication, assembly, and serviceability decisions.

## System Definition

| Requirement | Value |
|---|---|
| Robot type | Hexapod spider robot |
| Number of legs | 6 |
| Degrees of freedom per leg | 3 |
| Total servos | 18 |
| Servo model | DS3218 |
| Structural material | PETG |
| Fasteners | M3 hardware |
| Manufacturing method | FDM 3D printing |
| Target assembled weight | Under 5 kg |

Each leg shall comprise a coxa, femur, and tibia joint. Each joint shall be driven by one DS3218 servo.

## Dimensional Constraints

- The six leg mounting locations shall be arranged symmetrically about the body centerline, with three mounting locations on each side.
- Each leg shall provide three non-interfering rotary axes: coxa yaw, femur pitch, and tibia pitch.
- Final link lengths, body footprint, and stance height shall be defined in CAD before fabrication and shall support a stable standing pose without leg-to-leg, leg-to-body, or foot-to-body interference throughout the intended gait envelope.
- All dimensions that interface with DS3218 servos shall be verified against the physical servos used for the build. CAD shall not rely solely on marketplace dimensions or generic servo models.
- Servo pockets shall provide a minimum 0.5 mm assembly clearance around the measured servo envelope, except at positive locating features.
- Printed holes intended for M3 fasteners shall be designed at 3.2 mm nominal diameter unless a different post-processing method is explicitly documented.
- Clearances for moving parts shall be at least 0.5 mm after accounting for FDM dimensional variation.
- Cable passages shall be sized to allow installation and removal of the specified cable and connector without cutting, pinching, or excessive bending.
- The body shall include a protected internal electronics bay sized for the Raspberry Pi 4B, ESP32 DevKit V1, PCA9685 servo drivers, power-distribution hardware, connectors, and service access. Final bay dimensions shall be documented in CAD and validated against the actual selected hardware.
- The completed robot, including battery and all electronics, shall remain below 5 kg.

## Structural and Assembly Requirements

- The frame shall be printed in PETG and shall use M3 hardware for removable structural joints and electronics mounting where practical.
- Load-bearing servo mounts shall restrain each DS3218 servo against rotation and translation without relying on the servo output shaft or mounting screws alone.
- Leg segments shall transfer operating loads through the printed structure and M3 fasteners; servo output shafts shall not be used as the sole structural support for a joint.
- Every leg shall be replaceable independently of the other five legs.
- Leg removal shall not require disassembling the central body, removing unrelated legs, or cutting cable ties.
- The robot shall include a modular electronics bay with removable covers or modules for the Raspberry Pi, ESP32, PCA9685 boards, power system, and wiring interfaces.
- Assembly shall use a documented fastener schedule identifying fastener type, length, quantity, and location.
- Threaded connections in printed plastic shall use captive nuts, heat-set inserts, or other documented reinforcement where repeated service is expected.
- Assembly shall allow neutral-position servo calibration before final horn attachment and shall provide access to servo horn screws.

## Printability Requirements

- Parts shall be manufacturable by FDM 3D printing in PETG without unsupported enclosed features that cannot be cleaned or inspected.
- Each part shall specify its intended print orientation, layer height, infill, wall count, supports, and post-processing requirements in the manufacturing documentation.
- Load-bearing parts shall orient layers to reduce the risk of delamination under expected bending and torsional loads.
- Design features requiring bridges, supports, or tolerance-critical holes shall be validated with a small print before full production.
- Sharp internal corners in load-bearing members shall be avoided or relieved with fillets to reduce stress concentration.
- Large parts shall be split into printable sections only when the resulting joints maintain structural rigidity and remain serviceable.
- Parts shall include identification markings or a documented naming convention to prevent left/right and leg-position assembly errors.

## Cable Routing and Electronics Requirements

- Signal and power wiring shall route internally through the body and, where practical, through protected leg paths.
- Cable routing shall avoid moving joint pinch points, sharp edges, hot surfaces, and high-strain locations.
- Wiring shall include strain relief at body exits, servo interfaces, and electronics-bay entries.
- Power wiring shall be separated from sensitive signal wiring where practical to reduce electrical noise.
- Covers and cable guides shall be removable with M3 hardware for inspection and repair.
- Wiring paths shall preserve adequate bend radius for the installed cable and connectors.

## Maintenance and Serviceability Requirements

- Servos, servo horns, fasteners, electronics modules, battery connections, and cable routing shall be accessible with standard hand tools.
- A failed servo shall be replaceable without destructive disassembly of printed parts.
- Components subject to wear, impact, or frequent adjustment shall be replaceable as individual modules.
- The design shall permit inspection of fastener tightness, cable condition, servo mounting, and printed-part cracking without full robot teardown.
- Service documentation shall include exploded views, wiring routes, part numbers, print files, fastener schedule, and servo calibration procedure.
- After maintenance, the robot shall support repeatable reassembly to the documented neutral pose and leg geometry.
- Design revisions shall preserve backward-compatible interfaces where practical, especially for replaceable legs and electronics-bay modules.

## Acceptance Criteria

A mechanical revision is acceptable when it:

1. Uses six three-degree-of-freedom legs and 18 DS3218 servos.
2. Is fabricated primarily from PETG FDM-printed parts with M3 hardware.
3. Keeps the fully assembled robot under 5 kg.
4. Provides a replaceable-leg design, modular electronics bay, internal cable routing, and serviceable components.
5. Demonstrates non-interfering joint motion in the intended operating range.
6. Provides documented assembly, print, calibration, and maintenance instructions.