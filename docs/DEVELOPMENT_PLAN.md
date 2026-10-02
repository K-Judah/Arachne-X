# Repository audit and development plan

## Calibration persistence follow-up (2026-10-02)

From synchronized main 4d14f5317edad2339ed38934f25a12bffd339b41, the hardware-independent [calibration-record lifecycle](CALIBRATION_RECORDS.md) is implemented: bounded schema v1, CRC-32, complete configuration validation, external revision and approval-claim metadata, staged atomic publication, host memory storage and fault injection. All 97 previous cases remain; 34 calibration cases bring the total to 131 across seven passing Release suites. Physical output and upload guards remain locked. The existing wire protocol, remote prototypes and engineering assets are unchanged.

The simulated persistence recommendation below is now complete. Real calibration, evidence verification, revision assignment and physical storage durability remain unresolved. The next recommended phase is a host-only calibration inspection/review tool that displays record diagnostics and provenance without approving data or enabling motion. No such tool, geometry, IK, gait or hardware driver is implemented in this phase.

## Remote integration (2026-10-02)

The three local development changes were replayed onto remote main ee7eb541da199c8d636743de5a2dd22bb56c62e3, preserving its 69 commits since a94bafa. Remote changes have no file overlap with the local commits: CAD/Assembly revisions and additions, six OpenCV scripts plus their README, and Dashboard/dashboard.html remain unchanged. Documents/, Electronics/, firmware, root README, AGENTS.md, docs/ and CI had no remote changes in that interval.

The remote tree now includes tibia/chassis models and updated leg assemblies; file presence does not establish mechanical validation. OpenCV prototypes provide camera/HUD, HSV/colour, contour/motion and displayed steering experiments. The dashboard draft sends HTTP /cmd requests directly to an assumed ESP32 host; it has no implemented backend here and is not compatible with protocol v1. Both prototypes are preserved, not wired into control or exercised against hardware. Future integration must follow the safety boundaries without assuming those draft endpoints or camera settings are production choices.

The inventory, CAD counts and findings below describe a94bafa only, not the integrated tree. The original local development chain remains on backup/codex-protocol-f7af828. No new development phase is part of this integration.

Integration validation: a fresh MSVC Release build passed with /W4 /WX and all 97 cases across five CTest suites passed (33 control, 4 locked, 29 protocol, 2 protocol-locked, 29 Python/integration). Compile-conflict and upload guards passed. All 29 protected remote files matched Git blob hashes; remote vision/dashboard and engineering documents were unchanged. All 21 local links in the integration/development documentation passed. ESP32 cross-compilation remains unverified because the toolchain is absent. The initial Windows build environment required duplicate PATH normalization and sandbox access for MSBuild FileTracker; no project build changes were needed. CMake reported only the expected unused CMAKE_BUILD_TYPE option for the multi-configuration Visual Studio generator; Release was selected explicitly at build/test time.

## Protocol follow-up

The phase after 7d2f00e implements [protocol v1](../protocol/PROTOCOL.md): bounded COBS/CRC framing, a portable ESP32 parser/dispatcher, simulated transport, configuration-aware target preflight, atomic multi-target validation, explicit simulation enable, E-stop latch, configurable liveness/freshness deadlines, status telemetry and a stdlib-only Python reference client. Tests exchange bytes with the actual C++ endpoint in both simulation and locked builds. Physical-output and upload guards remain unchanged. No physical link, gait, IK or Pi robot service is added.

Phase 2's host protocol subset is implemented; real service deployment, authentication, session-seed/revision persistence and measured timings remain future work. The prior cross-build gate remains outstanding because the SDK/toolchain is absent; per this phase's instructions, the slow download is not repeated. Before any hardware phase, close that build gate and verify board identity, power/cutoff and mapping/calibration. The next implementation phase is a simulated calibration-record lifecycle: versioned serialization, integrity checks and atomic persistence tests, retaining the physical-output lock until electrical/mechanical prerequisites are resolved.

## Firmware foundation follow-up

The implementation following commit 6c87383 adds an ESP-IDF application packaged by PlatformIO, a portable C++17 joint/configuration/actuator core, host fakes/tests and CI. See [Firmware/README.md](../Firmware/README.md) for build commands and restrictions. Explicit simulation enablement exists only in host tests; firmware cannot enable output. The exact board remains unverified, so its generic ESP32 target is compile-only and uploads are blocked.

Phase 1's software configuration and disabled-startup foundation is implemented; its original 37 host cases remain regression tests. ESP32 cross-compilation remains unverified because its toolchain download was interrupted after impractically slow transfer; this build gate remains outstanding (see the firmware verification snapshot). Physical geometry, board settings, calibration persistence/versioning and electrical validation are still unresolved; no hardware-ready claim is made. Geometry schemas are deferred until their mechanical inputs are established. The historical audit below remains a record of the pre-implementation tree, not a description of today's firmware files. The protocol subset is now implemented as described above; the cross-build gate still blocks hardware validation.

Audit date: 2026-10-02. Audited baseline: a94bafa (Update Progress Log for July 24, 2026). The working tree was clean before this documentation work. The audit covered all 42 tracked files: all text files were read; CAD and media were inventoried and hashed without modification. CAD binary internals, assembly constraints and image content were not engineering-validated.

## Historical architecture and development status (a94bafa)

The repository is a mechanical prototype and project-planning collection. The documented intended chain is dashboard -> Raspberry Pi -> ESP32 -> PCA9685 -> servos. There is no executable firmware, Pi service, computer-vision implementation or dashboard source. Firmware/README.md describes an intended home for ESP32 and Pi code, not code currently present.

README.md describes a single-leg prototype in development. Documents/Progress_Log.md, dated 2026-07-24, records coxa/femur completion, an assembly constraint fix, an incomplete assembly and a tibia still in progress. Those statements are more specific than the master specification's broad claim that leg CAD v1 and hardware selection are completed. No physical validation or software results are recorded. The historical word "Tomorrow" is not a current schedule.

The current user-supplied baseline is 6 legs, 3 DOF each, 18 MG996R servos, Raspberry Pi 4B, ESP32-WROOM-32U based development board, two PCA9685 controllers and camera-based vision. Procurement/installation status remains unverified. See [software architecture](SOFTWARE_ARCHITECTURE.md) for the proposed boundaries and configuration policy.

## Historical inventory and findings (a94bafa)

| Area inspected | Evidence and finding | Disposition |
| --- | --- | --- |
| README.md | Goals/team information remains useful; DevKit V1 and generic actuator/driver descriptions are outdated or underspecified | Correct only the hardware baseline and clarify software status; add links |
| All MASTER_SPECIFICATION.md files | Eight byte-identical copies in Ai/, CAD/, Dashboard/, Documents/, Electronics/, Firmware/, Images/, Videos/ | Preserve; designate current software baseline in docs/ and plan later consolidation |
| Historical hardware references | All eight masters use DS3218 and DevKit V1; Documents/MECHANICAL_REQUIREMENTS.md also uses these, including DS3218-specific fit and acceptance requirements | Current baseline supersedes these model references for software; mechanical fit/load revalidation required |
| Documents/COMPONENTS.md | One PCA9685, generic servo model, and planned/need-more procurement labels | Current baseline requires two controllers and 18 MG996R; do not treat old status as verified inventory |
| Documents/ELECTRONICS_ARCHITECTURE.md | Only a block chain; no schematic, bus addresses, pinout or power interfaces | Retain concept; wiring and electrical design TBD |
| Documents/HARDWARE_ARCHITECTURE.md | Whitespace-only placeholder | Leave intact; fill in a later hardware documentation task |
| Documents/POWER_SYSTEM.md | Outline labels only: battery, rails, converters, current/runtime | No validated power budget, sensing, protection or cutoff specification |
| Documents/MECHANICAL_REQUIREMENTS.md | Substantial design requirements: PETG, M3, below 5 kg, serviceability, clearances and fabrication | Preserve engineering work; do not confuse documented requirements with measured geometry or proof of feasibility |
| Documents/ROADMAP.md | Ten phase headings; master copies contain seven phases with different ordering/scope | Historical roadmaps lack software dependencies and acceptance gates; phased plan below governs proposed software work |
| Documents/Progress_Log.md | Repeated short and detailed July 24 status; explicitly incomplete assembly/tibia | Preserve as historical evidence; no need to deduplicate during this task |
| Firmware/ | Master copy and two-line README only | Missing source, board configuration, drivers, protocol, control/safety logic and tests |
| Computer_Vision/ | Directory absent | Feature/COMPUTER_VISION/1 and Ai/Ai are empty placeholders, not CV code |
| Dashboard/ | Master copy and whitespace-only Dashboard file | No application, API contracts or build; Images/ UI artwork is a concept asset |
| Electronics/ | Master copy and whitespace-only Electronics file | No electronics design files or verified harness map |
| Feature/ | CAD_REDESIGN/1, COMPUTER_VISION/1, DASHBOARD_UI/1, ESP32_FIRMWARE/1 are whitespace-only | Directory markers, not implementations; keep pending scoped cleanup |
| Ai/ | Duplicate master and whitespace-only Ai file | Naming overlaps proposed CV ownership; avoid two active implementations |
| Images/ and Videos/ | Four image assets, duplicate masters; no video assets tracked | Preserve all media; presence does not establish implemented functionality |
| Build/test/release infrastructure | No build manifests, dependency locks, tests, CI workflows, simulator, runtime configuration schemas, setup commands or .gitignore tracked | Add with the first real software slice; no software build/test can currently be run |

The eight master files share SHA-256 C8B2FEEB40C73100A46F31CE24B15AB33E9BA7BF1C8B68C4344309E9C8809726 at the audited baseline. There are 13 whitespace-only text placeholders: Ai/Ai, Assembly/README.md, four CAD subdirectory READMEs, Dashboard/Dashboard, Documents/HARDWARE_ARCHITECTURE.md, Electronics/Electronics, and the four Feature/*/1 files. Firmware/README.md and POWER_SYSTEM.md are minimal descriptions/outlines, not empty files.

## Historical CAD structure and preservation (a94bafa)

```text
CAD/
  MASTER_SPECIFICATION.md
  Coxa/   README.md, Spider_Robot_Coxa_V1.ipt, Spider_Robot_Coxa_V1.stl
  Femur/  README.md, Spider_Robot_Femur_V1.ipt, Spider_Robot_Femur_V1.stl
  Servo/  README.md, Servo Part 1.ipt, Servo Part 2.ipt,
          Servo Pin.ipt, Servo Pin.stl
  Tibia/  README.md only
Assembly/
  README.md
  Spider_Robot_Leg_Assembly_V1.iam
```

There are nine CAD/assembly binaries (five .ipt, three .stl, one .iam), no tracked .dwg, and four image assets. Recent history includes explicit deletion of the old tibia .ipt and .stl; do not restore obsolete models automatically. Assembly dependency resolution and MG996R compatibility remain unverified. No chassis model, final geometry table, print settings or completed six-leg assembly is present in the tracked inventory. Do not infer that current coxa/femur models fit MG996R without measuring/reviewing them.

This task preserves every existing engineering file and asset except the narrow README correction. Duplicate master documents remain unchanged intentionally. A later documentation consolidation should preserve unique requirements/history, choose one canonical engineering specification, replace duplicate copies with links, and update hardware references in a reviewed change. No CAD reorganization is required for software work.

## Conflicts and unresolved decisions

| Decision | Evidence/current resolution | Required evidence before use |
| --- | --- | --- |
| Servo model | User baseline MG996R replaces historical DS3218 | Physical dimensions, usable travel, installed limits and load testing |
| Controller quantity | User baseline two PCA9685 replaces COMPONENTS.md quantity one | Actual boards, unique addresses and harness continuity |
| ESP32 board | WROOM-32U based board replaces generic DevKit V1 reference | Exact board revision, pinout, USB interface and boot/output-enable behavior |
| Camera | Vision required; Camera Module 3 appears only in old masters | Confirm camera model/interface and calibration |
| IMU/ToF | README originally listed them without status | Optional planned sensors; exact models, necessity and wiring TBD |
| Mechanics | Six 3-DOF legs documented; tibia/assembly incomplete | Revisioned link lengths, joint axes, mounting transforms, collisions and mass/centre of mass |
| Power | LiPo mentioned historically; detailed power document is an outline | Battery/rails, current budget, protection, grounding, regulator capacity and physical cutoff TBD |
| Performance | No validated timing or runtime figures | Measure motion period, stop latency, serial budgets, vision throughput and power under load |
| Telemetry | Master promises battery percentage, speed, leg/servo status | Distinguish commanded/estimated data from measured feedback; unavailable until sensing exists |

No unresolved physical value should be filled with a plausible number. Keep documentation values TBD and future machine-readable values explicitly unresolved; fail configuration validation before hardware arming.

## Phased implementation and acceptance gates

### 0. Confirm interfaces and physical evidence

Record exact board/camera identities, wiring proposal, two-controller address plan, servo inventory and electrical power/cutoff design. Have the mechanical work provide revisioned geometry and verify MG996R fit. Software scaffolding can proceed with synthetic fixtures; physical motion cannot proceed with unresolved power, mapping or calibration.

Exit: reviewed interface checklist; every unresolved field is explicitly TBD with the measurement needed to resolve it. Historical documentation conflicts are visible, not silently copied into code.

### 1. Buildable software foundation (implemented subset; see follow-up above)

Create only the ESP32 application skeleton and portable control/configuration test harness from the proposed layout. Select and pin the ESP-IDF/toolchain version and exact board settings after board identification. Introduce geometry/wiring/calibration schemas, the 18 named joint IDs, fake servo/clock interfaces, a disarmed startup state, dependency/build metadata, .gitignore and CI. Real PWM output remains disabled.

Exit: documented clean-checkout host test command and firmware build command pass; tests reject missing geometry/calibration, duplicate channels and invalid values; fake output proves boot and invalid configuration cannot energize joints. CI runs the same commands. No walking, vision or full dashboard is part of this task.

### 2. Protocol and Pi control skeleton (host protocol subset implemented)

Freeze exact framing/CRC/field definitions and golden vectors. Implement bounded parsers on both sides, session handshake, freshness, idempotent stop/disarm, command arbitration and telemetry using simulated devices. Add Pi package metadata, pinned dependencies and service setup instructions.

Exit: cross-language golden-vector and integration tests pass for normal exchange, corruption, truncation, duplicates, stale/out-of-order packets, queue saturation, process restarts and link loss. No old motion resumes; ACK acceptance is distinguished from completion. Define timeout values based on measurements before live control.

### 3. One-servo output and calibration bench

Implement PCA9685 adapter, explicit OE handling and versioned calibration persistence. Validate board identity/channel routing and power design before a supported one-servo test. Measure frequency/pulse behavior and establish conservative limits rather than sweeping assumed endpoints.

Exit: recorded pulse measurements, channel identity and calibrated neutral/direction/limits for the test servo; power-on/reset/disconnect/fault behavior demonstrated; interrupted configuration writes fail safely. Physical cutoff is independently verified. Live testing follows a deliberate bench procedure.

### 4. One-leg kinematics and motion validation

After tibia/assembly geometry is reviewed, implement pure FK/IK and bounded joint trajectories. Calibrate the three installed servos, verify transforms and compare predicted/measured poses on a supported leg.

Exit: host tests cover reachability, singularities, mirrored conventions, branch continuity and limit rejection. Recorded bench results demonstrate collision-free operation within a documented envelope and repeatable stopping. Synthetic test dimensions never become robot defaults.

### 5. Six-leg mapping, stance and crawl gait

Verify all 18 channels and per-servo calibration, then implement stance/swing scheduling and controlled transitions. Validate chassis geometry, load capacity, centre of mass and power delivery. Start in simulation, then supported static stance, then slow crawl; defer tripod gait until justified.

Exit: no duplicate/missing joints; gait continuity/support checks pass; configured timeouts and fault stop policy pass under lost commands, partial I2C failure and Pi reboot. Record acceptable current, temperature, stability and motion envelope before unsupported walking. Thresholds remain TBD until engineering validation.

### 6. Operator dashboard and telemetry

Build a minimal UI over the Pi API: arm/disarm/stop, explicit mode/lease, connection freshness, faults and commanded posture. Add camera preview when capture works. Preserve existing visual assets; use them only as design references.

Exit: API authorization and lease expiry tests pass, browser disconnect stops refreshing operator commands, stale/unavailable values are labelled, and UI cannot bypass firmware limits or falsely show measured joint feedback.

### 7. Vision and bounded autonomous behavior

Implement camera acquisition and recorded-frame replay before detector/tracker integration. Benchmark on Pi 4B. Select model/runtime using measured accuracy, latency, memory and licensing. Keep autonomy behind the same arbiter as manual control; start with observation-only mode.

Exit: reproducible evaluation clips/metrics, bounded frame queues, stale-detection rejection and CPU-load isolation. Tracking/following requires a separately validated distance/obstacle strategy and operating envelope. SLAM, patrol and return-home remain later research work, not promised functionality.

## Verification of this documentation change

Review the complete staged diff, check local Markdown links and run git diff --check. Compare pre/post SHA-256 hashes of all nine CAD/assembly binaries and four images, and confirm that only AGENTS.md, README.md and the two new docs/ files changed. No executable tests exist at the audited baseline; documentation validation is not a hardware or software runtime test. Commit the four documentation files with a descriptive message after those checks.
