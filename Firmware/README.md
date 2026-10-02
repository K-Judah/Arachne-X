# ESP32 firmware foundation

**Physical servo actuation is intentionally unavailable. This is not production-ready robot firmware.** No PCA9685, I2C, GPIO, PWM or transport driver exists. The ESP32 entry point constructs a disabled controller with unresolved configuration and returns. Building does not flash hardware, and PlatformIO upload targets are rejected. No hardware is needed for any test below.

The baseline is six three-joint legs, 18 MG996R servos and two PCA9685 controllers, with a Raspberry Pi 4B eventually supplying intent to an ESP32-WROOM-32U based board. The exact development board remains TBD. A hardware-independent [protocol v1](../protocol/PROTOCOL.md) now connects a [Python reference client](../Raspberry_Pi/README.md) to the host-tested C++ endpoint. Its component is registered for the ESP32 build but is not attached to a physical transport or boot task.

## Layout and build decision

```text
Firmware/
  platformio.ini          pinned ESP-IDF cross-compilation environment
  requirements-build.txt  pinned PlatformIO frontend
  CMakeLists.txt          host/ESP-IDF build selection
  control_core/
    include/arachne/      joint IDs, configuration, output/clock interfaces, actuator API
    src/                  portable validation and actuator implementation
  esp32/main/             inert app_main; no hardware drivers
  protocol/               bounded wire codec, transport interface and dispatcher
  calibration/            versioned record codec, persistence contract and host memory storage
  test/
    support/              fake output, fake clock, synthetic calibration, test runner
    test_control.cpp      simulation and validation tests
    test_locked.cpp       firmware-lock tests, including the actual app_main
  scripts/deny_upload.py  rejects PlatformIO upload/program targets
```

The existing architecture specifies ESP-IDF and separate esp32/control_core directories. We retain both, using PlatformIO as an ESP-IDF dependency/build frontend, not switching to Arduino. Host CMake/CTest compiles the same core without downloading an embedded SDK or a test framework. The Python reference client lives outside the firmware project in Raspberry_Pi/; a full Pi robot service is not implemented.

## Run host tests

Prerequisites: CMake 3.20 or newer, Python 3.11+ (standard library only) and a C++17 compiler (GCC/Clang, or Visual Studio C++ Build Tools and a Windows SDK). The host tests have no external library dependencies. From the repository root:

```sh
cmake -S Firmware -B Firmware/build-host -DARACHNE_HOST_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build Firmware/build-host --config Release --parallel
ctest --test-dir Firmware/build-host -C Release --output-on-failure
```

On Windows, use a developer terminal if the tools are not on PATH. CMake can use its default Visual Studio generator; do not reuse a build directory generated for a different compiler. Use `ctest --test-dir Firmware/build-host -C Release -V` to see every named test case. Tests use runtime checks that remain active in Release builds, not C/C++ assertions removed by NDEBUG. Project sources and tests compile with warnings treated as errors.

`control_tests` links the simulation-enabled core; `locked_tests` separately compiles the same core with simulation disabled, exactly as firmware does. The latter proves that even complete synthetic configuration and a fake backend cannot enable the locked build. `protocol_tests` and `protocol_locked_tests` exercise the codec/dispatcher. A Python CTest entry runs unit tests and actual Python-to-C++ endpoint exchanges through two host-only bridge executables. Two additional suites, calibration_tests and calibration_locked_tests, cover persistence and restored-configuration safety, for seven suites total. See [calibration records](../docs/CALIBRATION_RECORDS.md) for the schema, lifecycle and validation results. Configuration also verifies that ESP32 flags combined with simulation enablement fail compilation for the expected guard diagnostic. The Python suite verifies the unchanged upload guard. The GitHub Actions workflow runs all host tests on Linux and Windows and separately cross-compiles the ESP32 application; no physical devices are used.

## Cross-compile only

Use Python 3.11 and install the pinned frontend in a virtual environment if PlatformIO is not already available:

```sh
python -m pip install -r Firmware/requirements-build.txt
pio run --project-dir Firmware --environment esp32_compile_only
```

The environment pins PlatformIO Core 6.1.19, Espressif32 platform 6.10.0, ESP-IDF package 3.50400.0 (ESP-IDF 5.4.0) and Xtensa compiler 14.2.0+20241119. First compilation downloads SDK/tool packages and requires internet access. Other build helpers are resolved by that platform release. See the [PlatformIO ESP-IDF build documentation](https://docs.platformio.org/en/latest/frameworks/espidf.html) and [pinned platform manifest](https://github.com/platformio/platform-espressif32/blob/v6.10.0/platform.json).

`esp32dev` is a generic ESP32 **compile-only surrogate**, not a claim that the installed board is a DevKit V1 or has the manifest's flash layout. No robot GPIO, I2C address, oscillator frequency or production calibration is assigned. Actual board/flash settings must be verified before a later deployment phase. Do not flash the generated image. Upload is intentionally blocked; direct external flashing tools are outside this project's safeguards.

## Joint identity and configuration

Strong enums define `Leg::{LF,LM,LR,RF,RM,RR}`, `JointKind::{Coxa,Femur,Tibia}` and all 18 logical `JointId` values, from `LF_Coxa` through `RR_Tibia`. `Unknown` is a rejected sentinel used by default-constructed records, not an additional joint. IDs follow the architecture's leg-major order; names such as `LF.coxa` are available for diagnostics. Bounds-checked helpers reject invalid leg/joint enums.

Each `JointConfiguration` links one logical ID to a `Wiring` record (controller A/B and channel) and a separate `Calibration` record. Configuration slot order does not determine identity. The only production factory, `unconfigured_robot()`, contains the 18 identities but leaves all wiring, direction, angles and pulses as `std::nullopt`, with `Uncalibrated` state. There is no guessed mapping or default calibration in firmware. The architecture's channel table remains a proposal, not installed wiring.

Calibration fields use explicit radians and microseconds: minimum angle, neutral angle/offset, maximum angle, minimum pulse, neutral pulse and maximum pulse, plus Normal/Inverted direction and calibration state. Neutral must lie strictly inside both ranges so the two interpolation segments are defined. `neutral_rad` is the logical joint coordinate at the measured neutral pulse; it is not implicitly zero. Inversion reverses pulse endpoints while preserving measured neutral. The controller rejects targets outside the calibrated logical range rather than silently clamping them.

`validate()` returns a bounded `ValidationReport`, with an error code, offending configuration slot where applicable, logical joint, and a human-readable `describe()` message. It reports missing/duplicate/invalid joints, missing/invalid controllers and channels, duplicate controller/channel pairs, missing calibration fields, uncalibrated/invalid state, invalid direction, non-finite values, reversed/equal ranges and neutral values outside the strict interior. The PCA9685's 0-15 channel domain is a device constraint, not a chosen harness assignment. The same channel number on A and B is valid. Any invalid joint blocks enabling the entire system.

Only test code contains a complete configuration. Its deliberately asymmetric numbers are **synthetic arithmetic fixtures, not MG996R pulse ranges or robot joint limits**. Test support is outside the firmware component and compilation rejects inclusion on ESP32. Geometry, I2C addresses, GPIO assignments, PWM frequency, mechanical limits and measured offsets all remain TBD; fields not consumed by this phase are deliberately absent rather than given misleading defaults. Geometry schemas and physical storage adapters remain future work before physical motion. The portable calibration record and simulated atomic persistence are implemented; all physical values remain unresolved.

## Safety boundary and interfaces

`ServoOutput` accepts a typed `PulseCommand` (logical joint, controller, channel, pulse in microseconds, monotonic timestamp). `Clock` supplies optional monotonic microseconds; unavailable time fails closed. `FakeOutput` records attempts/accepted commands and can inject failure; `FakeClock` supplies deterministic time. `DisabledOutput` rejects every write without touching hardware. This phase does not convert pulses to PCA9685 counters or configure PWM frequency.

`ActuatorSystem` copies configuration into an immutable snapshot. Construction and `start()` issue no output. State begins `Disabled`; only explicit `enable_simulation()` in a host simulation build can transition to `SimulationEnabled`. That method requires complete valid configuration, an explicitly simulated backend and an available clock. Enable itself sends nothing. `disable()` and `start()` block subsequent commands; they never move to neutral or replay a previous command.

Firmware builds contain no enabling path. Defining `ARACHNE_HOST_SIMULATION` together with ESP-IDF's `ESP_PLATFORM` produces a compile error. The host simulation target alone defines that macro. There is no runtime switch, calibration flag or configuration value that unlocks the ESP32 build. A backend reporting Physical or Unavailable is rejected even in simulation. Test fakes have no hardware dependencies.

Invalid direct actuator targets, invalid runtime backend domain, clock loss/rollback and failed writes latch `Fault`; subsequent commands and enable requests fail. Protocol preflight uses the side-effect-free `validate_target()` to reject an entire invalid request before actuator calls; `emergency_stop()` explicitly latches Fault. Start/disable cannot clear that fault. For a fresh offline test session, construct a new controller; a production recovery protocol is not yet implemented. The actuator API is single-thread-owned and must not be called concurrently. No timing loop, velocity limiter, physical watchdog, IK or gait is implemented. The protocol endpoint implements configurable communication/motion deadlines only when its owner polls/ticks it; those are not a physical watchdog.

Stopping future software writes is **not** a physical emergency stop, power disconnect or guarantee that an externally powered PCA9685 has stopped an old waveform. This phase must never be used to control powered actuators. OE/cutoff wiring and a measured stop policy are prerequisites for any future hardware driver.

## Future calibration

After the electrical design, cutoff circuit and actual harness are verified, support the mechanism and calibrate one joint at a time: confirm channel identity, establish neutral before fitting the horn, measure direction and conservative collision-free endpoints, and validate the installed joint. Never blindly sweep assumed MG996R endpoints. Record servo identity, CAD/geometry revision, measurement method and date; use the versioned calibration record and atomic storage contract to retain approved evidence. A durable physical storage adapter and a verified approval process remain future work. Servo/horn replacement or changed geometry invalidates the affected record. The current `Calibrated` enum is sufficient for synthetic tests, not evidence of physical certification.

## Foundation verification snapshot (commit 7d2f00e, 2026-10-02)

- Windows x64, CMake 4.3.1-msvc1, MSVC 19.51.36260.0: Release configuration and compilation passed with /W4 /WX; no compiler warnings or errors.
- CTest: 2/2 executables passed, comprising 33/33 control cases and 4/4 locked-build cases (37 total). The actual ESP32 entry-point source was also compiled and run against the locked host core.
- CMake's negative compile check rejected ESP32 plus simulation flags with the intended diagnostic. Five isolated Python checks of the upload guard accepted normal build targets and rejected upload/uploadfs/program without invoking devices.
- PlatformIO 6.1.19 cross-build was attempted but interrupted during the first Xtensa toolchain download (380,779,175-byte archive). A separate 1 MiB transfer took 83.5 seconds. No ESP32 image was compiled or verified; rerun the documented cross-build when the SDK/toolchain download is available. The CI workflow is committed but has not been run on GitHub in this session.
- Protected CAD/Assembly/Images/Videos files passed before/after SHA-256 comparison. No physical hardware was used. These results do not establish electrical, mechanical or real-time safety.
