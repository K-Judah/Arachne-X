# Proposed software architecture

Status: architecture proposed 2026-10-02; the [firmware foundation](../Firmware/README.md) now implements portable joint/configuration validation and host-only simulated actuator control. The ESP32 application remains inert and physical output is unavailable. Gait, IK, drivers, protocol and Pi services below remain proposals. See [development plan and repository audit](DEVELOPMENT_PLAN.md) for evidence and sequencing. Existing Documents/ files remain engineering history; this proposal uses the hardware assumptions supplied for the audit.

## Hardware and configuration contract

| Item | Current baseline |
| --- | --- |
| Legs and joints | 6 legs, coxa yaw / femur pitch / tibia pitch; 18 joints |
| Actuators | 18 MG996R servos |
| High-level computer | Raspberry Pi 4B |
| Low-level computer | ESP32-WROOM-32U based development board; exact board/pinout TBD |
| PWM controllers | 2 PCA9685 boards; actual addresses and wiring TBD |
| Vision | Camera-based; camera model, interface and calibration TBD |
| Mechanics | Evolving CAD; link lengths, mounting transforms, joint zeros and motion envelope TBD |
| Power and sensing | Rails, battery, current budget, cutoff circuit, IMU/ToF selection and feedback sensors TBD |

PETG, M3 hardware and a below-5-kg target appear in the historical mechanical requirements. They are design requirements, not measured properties of a completed MG996R robot. Servo fit, torque/load capacity and clearances require revalidation. Do not infer link lengths from filenames or use generic servo travel/pulse specifications as calibration.

## Responsibilities and data flow

```text
Browser dashboard <-> Pi API / telemetry service <-> command arbiter
Camera -> vision worker -> timestamped observations -> mission controller
Mission controller ---------------------------------> command arbiter
command arbiter <-> serial link <-> ESP32 protocol / safety supervisor
                                   -> gait -> foot targets -> leg IK
                                   -> joint limiter -> calibration -> PCA9685 x2
                                   -> 18 servos
```

The Pi sends bounded motion intent, not continuous PWM writes. The ESP32 maintains timing and independently rejects unsafe/stale requests. Vision cannot directly drive motors. Manual and autonomous sources share one exclusive control lease; stop/fault handling overrides both. A Pi crash must not leave an indefinite command active.

Proposed stack: ESP-IDF with C++ portable control components on ESP32; Python packages for Pi services and vision; TypeScript browser UI. Select and pin supported versions when scaffolding, after confirming the exact board and Pi OS. Start with simple processes and typed contracts; defer ROS integration until navigation needs justify it.

## Proposed repository layout

```text
Firmware/esp32/           ESP-IDF application, board adapters, component tests
Firmware/control_core/   portable safety, kinematics, gait and joint logic
Raspberry_Pi/            Python command arbiter, serial client, API and missions
Computer_Vision/         camera adapters, inference workers, replay and evaluation
Dashboard/              browser application and UI tests
protocol/               versioned wire specification, schemas and golden vectors
config/                 geometry, wiring and calibration schemas/examples
simulation/             fake plant, visualizer and scenario runner
tests/integration/      Pi/ESP32 contract and fault-injection scenarios
docs/                   software decisions, setup and validation procedures
Documents/              existing engineering records (preserved)
```

The audit created docs/. The next phase adds Firmware/esp32/, Firmware/control_core/ and Firmware/test/, with PlatformIO wrapping the specified ESP-IDF framework and CMake/CTest for independent host tests. The generic ESP32 build target is compile-only until the exact board is identified; physical enable is compiled out. Geometry/persistence schemas and the complete safety state machine remain future work. Keep existing Ai/ and Feature/ placeholders until a separate consolidation change. Avoid parallel implementations in Ai/ and Computer_Vision/.

## ESP32 firmware and PCA9685 output

Use a fixed-period motion task driven by a monotonic clock. Its period, command refresh budget and watchdog thresholds are TBD until measured. Protocol parsing and telemetry run outside the motion task with bounded queues and payload sizes. Never block motion on logging or network activity. One component owns I2C; report write failures and deadline misses to the safety supervisor.

The pipeline accepts a desired body motion/posture, updates gait phase, computes six foot targets, solves IK, checks all 18 joints, applies rate limits, then converts angles to calibrated pulse widths. Reject an invalid full target before any output writes. Stage a complete output frame; two boards cannot be assumed to update atomically, so measure skew and fault on partial-write failure rather than reporting successful motion.

The PCA9685 adapter owns board discovery against configured identities, frequency setup, pulse-to-counter conversion and output-enable behavior. Account for measured oscillator/frequency error. Servo PWM frequency and approved pulse bounds are TBD bench values. Keep PWM carrier frequency separate from gait/control update frequency. Do not silently remap channels when a board is missing. Unused channels stay disabled. Proposed OE gating needs verified wiring and a defined boot/reset state before hardware tests; disabling PWM is not the same as disconnecting servo power.

## Stable 18-joint mapping

Use the robot's forward direction, never the camera image, to name left/right. Proposed IDs are stable software identities. This table is a proposed harness allocation, not verified wiring; addresses, pinouts, polarity and installed mapping remain TBD until continuity and single-joint tests pass. Each controller uses nine channels so all three joints of a leg remain together.

| Leg | IDs: coxa, femur, tibia | Proposed board | Proposed channels |
| --- | --- | --- | --- |
| Left front (LF) | 0, 1, 2 | A | 0, 1, 2 |
| Left middle (LM) | 3, 4, 5 | A | 3, 4, 5 |
| Left rear (LR) | 6, 7, 8 | A | 6, 7, 8 |
| Right front (RF) | 9, 10, 11 | B | 0, 1, 2 |
| Right middle (RM) | 12, 13, 14 | B | 3, 4, 5 |
| Right rear (RR) | 15, 16, 17 | B | 6, 7, 8 |

Use names such as LF.coxa in configuration and logs. Validate exactly 18 unique joints, unique board/channel pairs, valid channel ranges, two distinct configured board identities and complete per-joint calibration. Channels 9-15 on each board are reserved/disabled. Do not assume mirrored legs have identical servo signs.

## Calibration and geometry

Separate three versioned configuration records:

- Geometry: robot/CAD revision, source measurements, three link lengths, six body-to-leg transforms, joint axes, zero conventions, permitted workspace and collision constraints. Physical values TBD.
- Wiring: board identities/addresses, ESP32 pin assignments, named joint-to-channel map and output-disable wiring. Physical values TBD.
- Calibration: servo identity and installed joint, direction, neutral offset, validated angle-to-pulse points, pulse and angle bounds, velocity/acceleration limits, date and method. Physical values TBD.

Prefer a measured monotonic piecewise-linear angle-to-pulse curve when a simple offset/scale is inadequate. Reject non-finite, non-monotonic or incomplete records. Validate the configuration offline and again on the ESP32; transmit an immutable version/hash and refuse arm on mismatch. Persist validated records atomically with integrity checks; interrupted writes must leave a previous valid record or a disarmed fault, never partial calibration.

Calibration is a deliberate service mode: support the mechanism, verify supply/cutoff and channel identity, operate one joint at a time with restricted pulses, establish neutral before horn attachment, measure direction and conservative usable endpoints, then verify the installed joint without collision. Never sweep blindly to physical stops. Replacing a servo/horn or changing CAD invalidates affected calibration/geometry until rechecked. Raw PWM commands are excluded from normal operation.

## Kinematics and gait

Proposed body frame: right-handed, +X forward, +Y left, +Z up; use metres, radians, seconds and explicit frame IDs. Define each leg frame at its coxa mount using a configured rigid transform. Define positive joint angles by the right-hand rule about documented joint axes; servo electrical direction belongs in calibration.

Provide pure forward/inverse kinematics functions with geometry inputs and structured results: solution, unreachable, singular or joint-limit violation. A yaw plus planar two-link analytic solver is appropriate only after CAD confirms that geometry; otherwise use a model matching measured offsets/axes. Choose the solution branch nearest the previous valid pose with continuity constraints. Never hide unreachable targets by broadly clamping trigonometric arguments; allow only documented floating-point tolerance. Reject invalid motion and enter the tested stop policy. Test FK/IK round trips, mirror transforms, workspace edges, singularities and branch continuity using synthetic fixtures clearly separated from physical configuration.

Gait generation owns stance/swing scheduling, phase, bounded foot trajectories and transitions between stand, controlled stop and walking. Start with a slow crawl and explicit support-margin checks using a measured or conservatively bounded centre of mass; that value is TBD. Add tripod gait only after stability testing. Speed, step length, clearance, duty factor and body height are validated configuration, all TBD for hardware. Interpolate transitions; do not jump between gait poses. Quasi-static support checks do not prove dynamic stability or actual foot contact.

## Safety state machine

Proposed states: BOOT -> DISARMED -> READY -> ARMED, with CALIBRATION entered explicitly from DISARMED and FAULT latched from any state. READY means configuration, required devices and session handshake are valid; ARMED additionally requires an explicit operator request and a fresh control lease. Boot and reset keep outputs disabled. Leaving FAULT requires the cause to clear, explicit acknowledgement and a new arm sequence.

Validate finite values, units, limits, command age, sequence, mode and configuration before acceptance. A motion-command expiry, link loss, repeated deadline miss or driver failure triggers local fault handling even if heartbeat traffic continues. A heartbeat alone never refreshes motion intent. Reject malformed frames without refreshing any lease.

Distinguish controlled stop (bounded deceleration into a validated supported pose), output disable, and physical power cutoff. The appropriate fallback depends on support and power; disabling servos may collapse the chassis. A stop profile, maximum hold duration, escalation policy and timeouts are TBD and block loaded walking until bench-tested. Loss of trustworthy output control uses the verified hardware disable/cutoff path. Software cannot guarantee cutoff without that circuit. Never resume the last gait after reconnect.

## Pi to ESP32 communication

Propose wired serial, initially USB serial through the development board if supported; direct UART is an alternative after voltage levels, pinout and grounding are verified. Connector, baud rate and wiring are TBD. Transport adapters must allow offline loopback and replay.

Before implementation, freeze a versioned bounded binary frame: delimiter-safe framing (proposed COBS), protocol version, message type, payload length, boot/session ID, sequence, request ID, payload and CRC. Exact field widths, byte order, CRC parameters, maximum frame size and golden bytes must be specified in protocol/ before parser work. CRC detects corruption; it is not authentication.

Define HELLO/CAPABILITIES, CONFIG_STATUS, ARM, DISARM, STOP, MOTION_INTENT, HEARTBEAT, ACK/NACK, TELEMETRY and FAULT messages. Motion intent includes velocity/posture/gait and a bounded validity interval; use ESP32 monotonic receive time for local expiry, not unsynchronized Pi wall time. Add a receiver-issued freshness token with an ESP32 deadline so delayed previously unseen packets cannot gain a fresh lease simply by arriving late. Session IDs and sequences reject replay, duplicates and out-of-order commands. Negotiate protocol/configuration compatibility before enabling commands. Handshake or reboot invalidates old sessions and pending commands.

ACK identifies acceptance/rejection and reason; acceptance is not physical completion. Retry only operations with defined idempotency, deduplicated by request ID; do not replay queued motion after recovery. Keep only the latest valid motion intent. STOP has priority over queued movement. Report rejected commands and expiries. Bound parser memory, resynchronize after corrupt/truncated data, and rate-limit diagnostics. Authentication of remote operators belongs at the Pi API; serial access remains restricted to the robot service.

## Pi services, vision and dashboard

Run a supervised robot service with separate command arbiter, serial adapter, telemetry cache and API components. Restart into a non-driving state. Mission logic supplies intent only while its exclusive lease is valid; operator override cancels that lease. Persist structured events with bounded storage, session IDs and timestamps. Keep camera/inference in a separate worker so slow inference cannot stall control or telemetry.

Vision stages: camera capture -> timestamped frame -> optional preprocessing -> detector -> tracker -> observations. Use bounded latest-frame queues and drop stale frames. Camera intrinsics/extrinsics, detector/model, input resolution, inference runtime and frame-rate target are TBD after Pi 4B profiling. Keep frame timestamps/IDs on detections, confidence and tracking metadata. Monocular detections alone do not establish metric distance; report distance unavailable unless a calibrated method supports it. Test against recorded clips before autonomous behavior. Model artifacts need provenance, licensing and evaluation records; no model downloads are part of this audit.

The dashboard connects only to the Pi: HTTP for configuration/status operations, WebSocket for telemetry and control-lease updates, and a separate camera stream whose transport is selected after latency measurements. Provide authentication, command authorization and deployment guidance before remote control; limit initial service exposure to an explicitly configured local network. Browser disconnect and tab inactivity must expire the operator lease independently of Pi-to-ESP32 health.

Telemetry includes ESP32 uptime, state/fault, command age, configuration hash, controller errors, gait phase, commanded joint angles, Pi health, camera status and timestamped detections. Attach units, source and freshness. Label joint positions as commanded and foot positions as model-estimated unless sensors measure them. Battery percentage/runtime/current and servo health remain unavailable until sensing and estimation are validated. UI mockups in Images/ are concepts, not working features. Never display unknown subsystems as healthy by default.

## Simulation, testing and reproducibility

Use one portable motion implementation in firmware and host simulation to avoid divergent IK/gait algorithms. Start with kinematic visualization and a fake servo/I2C/clock backend; add dynamics only when mass, inertia, friction and actuator models are measured. Synthetic dimensions belong only in named test fixtures and cannot arm hardware. CAD is input reference material, not something tests rewrite.

Plan host tests for configuration rejection, all 18 mappings, pulse conversion/bounds, IK/FK, gait continuity and the safety state machine. Use shared protocol golden vectors on Pi and ESP32, parser fuzzing and fault injection for malformed/oversized frames, loss, duplicates, latency, reboot, clock behavior, partial I2C writes and stale vision/operator commands. Integration tests must show automatic motion does not resume after failure. Separate simulated behavior from physical evidence.

Add pinned build manifests, Python dependency metadata, frontend lockfile, documented commands and CI when each component is introduced. CI should build ESP32 firmware, run portable control/Pi tests, validate contracts/configuration, and later lint/type-check/test the UI. Hardware tests are opt-in bench procedures, never default CI. Archive configuration revision, software revision, test conditions and results for every physical milestone.
