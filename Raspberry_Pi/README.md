# Raspberry Pi protocol reference client

This source-only Python 3.11+ package implements the hardware-independent [protocol v1](../protocol/PROTOCOL.md) client. It is not a robot service, planner, physical transport or actuator driver. It has no third-party runtime/test dependencies; no package installation is needed to test it.

`arachne_protocol/wire.py` supplies bounded COBS/CRC framing and a stream parser. `transport.py` defines the nonblocking transport contract and bounded in-memory pairs. `client.py` implements handshake, heartbeat, status, explicit simulation enable requests, disable, single/multi targets and E-stop. Clients never enable automatically or retry requests. Physical permission remains false and a response claiming otherwise is rejected.

Add `Raspberry_Pi/` to Python's import path, then supply an implementation of `Transport` to `Client`. A memory pair alone needs its peer to service requests; it does not simulate firmware by itself. The test harness runs the actual C++ endpoint instead of a second Python server model. The current synchronous client expects a serviced reply when it reads; async IO/deadline scheduling belongs in a later transport adapter.

The API is `hello()`, `heartbeat()`, `status()`, `enable()`, `disable()`, `set_joint(joint_id, radians)`, `set_multi([(joint_id, radians), ...])`, and `emergency_stop()`. Accepted command results expose `accepted`, `simulation_enabled`, and `physical_permission` separately. NACKs raise `RemoteError` with an `Error` code and state/fault outcome. No target values are suggested here because actual geometry, travel and calibration remain TBD.

Run all previous firmware tests, new protocol tests and Python/C++ integration tests from the repository root:

```sh
cmake -S Firmware -B Firmware/build-host -DARACHNE_HOST_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build Firmware/build-host --config Release --parallel
ctest --test-dir Firmware/build-host -C Release --output-on-failure
```

The Python test command is also visible with `ctest --test-dir Firmware/build-host -C Release -N -V`. It needs paths to the two built protocol bridge executables; neither bridge connects to hardware. CTest limits the integration test duration. Physical transport, authentication, production timing, boot-session uniqueness and configuration revision persistence remain TBD.
