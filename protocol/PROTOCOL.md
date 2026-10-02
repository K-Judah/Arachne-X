# Arachne-X command protocol v1

Status: host-tested reference protocol, physical actuator output locked. This protocol connects a Python Raspberry Pi reference client to a portable C++ ESP32-side endpoint. It does not implement the Pi robot service, a physical transport, gait, IK, autonomous navigation or vision. Existing firmware simulation/physical-output guards remain authoritative.

## Layers and ownership

`wire` encodes/decodes bounded frames without any transport or control dependency. `Endpoint` owns a stream parser, session/freshness state and the dispatcher. It calls the existing `ActuatorSystem` only after validation. A new const `validate_target()` permits all targets to be checked against the actuator's own immutable configuration; `emergency_stop()` only latches its existing fault state. No output path is added to the firmware build.

`Transport` provides nonblocking byte reads (Byte/Empty/Closed) and all-or-fail writes. `MemoryTransport` is a fixed-capacity duplex FIFO. An owner must call `Endpoint::poll()` regularly, including while idle, or call `tick()` between polls; no background task or timeout thread is created. Each poll consumes at most 281 bytes. Closed links, failed writes and backpressure invalidate the session and disable further commands. The physical owner-loop period remains TBD.

Python's equivalent transport has `write(bytes)` and `read(maximum)`. The small synchronous reference client allows one outstanding request and never retries commands automatically. A reply must be available when read is serviced; an empty read or incomplete/corrupt response raises an error rather than waiting indefinitely. Scheduling, asynchronous IO and real-link deadlines belong in a future adapter. Neither implementation imports UART, USB, network, Bluetooth or PCA9685 APIs. The host bridge's text/hex process interface is a test harness, not a proposed robot transport.

## Exact wire representation

All multibyte integers are unsigned little-endian. Joint targets are IEEE-754 binary64, little-endian, in **logical joint radians**, not PWM counts or servo pulses. Binary64 preserves the existing double-valued calibration endpoints without binary32 rounding past a configured limit. NaN/infinity and out-of-range values are rejected; there are no implicit units, travel limits or clipping rules.

The complete on-wire frame is `COBS(header || payload || CRC16) || 0x00`. COBS replaces zeros with block-length codes: a code 1-254 represents that many minus one following nonzero bytes and an implied zero between blocks; code 255 represents 254 nonzero bytes without an implied zero. Only the terminating delimiter is zero on the wire. Encoders are deterministic and shared golden vectors define exact examples.

| Decoded offset | Bytes | Field |
| --- | --- | --- |
| 0 | 1 | Protocol version, exactly 1 |
| 1 | 1 | Message type |
| 2 | 4 | Request ID/sequence |
| 6 | 8 | Receiver-issued session ID; zero means no active session |
| 14 | 4 | Receiver-issued freshness token; zero means unavailable |
| 18 | 2 | Payload byte length, 0-256 |
| 20 | length | Payload |
| 20 + length | 2 | CRC-16/CCITT-FALSE over header and payload, stored little-endian |

CRC parameters: polynomial 0x1021, initial 0xFFFF, no input/output reflection, final XOR 0x0000; `123456789` gives 0x29B1. CRC is corruption detection, not authentication. There is no packed C++ struct or dependence on native padding/endianness.

Maximum decoded size is 278 bytes; maximum encoded size excluding delimiter is 280; maximum wire frame is 281. Payload length must exactly match decoded size. There is no padding or accepted extra trailing payload. Unsupported versions, unknown types, malformed COBS, CRC errors, oversize and truncation are rejected before dispatch. Their headers are untrusted: they receive no ACK/NACK and increment a saturating diagnostic counter. Consecutive empty delimiters are ignored. Overflow discards input through the next delimiter; the parser then resynchronizes.

A stream fragment is incomplete until its delimiter arrives; it is not prematurely executed. Explicit end-of-stream or the configurable frame-assembly deadline identifies an unterminated frame as truncated, discards it and disables/invalidate the session. `Parser::finish()` exposes truncation even without a clock/transport. Firmware parser/dispatcher storage is fixed-size; no dynamic allocation is used in those layers.

## Commands and payloads

| Type | Name | Exact payload |
| --- | --- | --- |
| 1 | HELLO | Minimum supported version u8, maximum supported version u8 (2 bytes) |
| 2 | HEARTBEAT | Empty |
| 3 | GET_STATUS | Empty |
| 4 | ENABLE_REQUEST | Expected configuration revision/tag u32 (4 bytes) |
| 5 | DISABLE | Empty |
| 6 | SET_JOINT_TARGET | Joint ID u8, logical angle binary64 (9 bytes) |
| 7 | SET_MULTI_JOINT_TARGET | Count u8 (1-18), then count target records of 9 bytes (10-163 bytes) |
| 8 | EMERGENCY_STOP | Empty |

IDs 0-17 follow the architecture: LF coxa/femur/tibia, LM, LR, RF, RM, RR, each in that joint order. IDs outside this set are invalid. Each multi-target joint must be unique. All message payload lengths, identities, calibration states, finite numeric values and configured limits are checked before forwarding. Every multi-target entry passes preflight before any actuator command is called. Invalid batches forward zero commands. A subsequent simulated output/clock failure during an otherwise valid batch latches a fault; already accepted backend writes cannot be rolled back. This is atomic validation at the protocol/control boundary, not transactional physical PWM.

## Responses

Every response uses version 1 and echoes the exact request ID. Its session/token fields reflect the receiver's current state (zero after invalidation). Response types received by the embedded endpoint are rejected with WrongDirection; responses never execute as commands.

| Type | Name | Payload |
| --- | --- | --- |
| 128 | HELLO_ACK | Selected version u8, configuration tag u32, heartbeat timeout u64 microseconds, command/freshness timeout u64 microseconds, physical permission u8 (22 bytes) |
| 129 | ACK | Common outcome record, below |
| 130 | NACK | Common outcome record with nonzero error |
| 131 | STATUS | State/telemetry snapshot, below |
| 132 | HEARTBEAT_REPLY | Common outcome record; header contains newly issued token |

Common outcome (5 bytes): echoed command type u8; error u8; actuator state u8; fault u8; physical permission u8. ACK/HEARTBEAT_REPLY use error 0. A successful enable ACK means **request accepted** and its state may be **SimulationEnabled**; **physical permission is always 0**. The locked firmware build returns NACK/PhysicalLocked even for valid synthetic calibration. ACK is not measured motion completion. The Python client rejects a response that claims physical permission.

Actuator states: 0 Disabled, 1 SimulationEnabled, 2 Fault. Fault codes: 0 None, 1 EmergencyStop, 2 HeartbeatTimeout, 3 CommandTimeout, 4 Clock, 5 Transport, 6 Actuator. Timeout/transport diagnostics can be cleared by a fresh successful handshake; the actuator's latched Fault cannot. State and fault are separate: a timeout can report Disabled plus its timeout reason.

Error codes: 0 Ok; 1 BadPayload; 2 UnsupportedVersion; 3 WrongDirection; 4 NoSession; 5 BadSession; 6 BadSequence; 7 StaleToken; 8 NotConfigured; 9 ConfigMismatch; 10 Disabled; 11 PhysicalLocked; 12 InvalidJoint; 13 InvalidValue; 14 OutOfRange; 15 Uncalibrated; 16 InvalidConfiguration; 17 DuplicateJoint; 18 Stopped; 19 ActuatorFailure. The first failed validation determines the error. Envelope/state checks precede target checks. HELLO version-range disagreement receives error 2; an unsupported frame-header version is dropped by the decoder.

### STATUS layout (249 bytes)

| Payload offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 1 | Actuator state |
| 1 | 1 | Fault |
| 2 | 1 | Physical permission, always 0 |
| 3 | 1 | Whole actuator configuration valid, 0/1 |
| 4 | 1 | Active protocol session, 0/1 |
| 5 | 1 | Configured controller mask: bit 0 A, bit 1 B; not hardware health |
| 6 | 1 | Joint count, 18 |
| 7 | 4 | Configuration revision/tag; 0 means TBD |
| 11 | 4 | Malformed-frame counter, saturating u32 |
| 15 | 234 | 18 joint records, each 13 bytes, sorted by ID |

Each joint record: joint ID u8; configured controller u8 (0 A, 1 B, 255 unknown); configured channel u8 (0-15, 255 unknown); calibration-state flag u8; commanded-target-valid flag u8; last successfully accepted commanded angle binary64. The angle is ignored when its flag is zero (wire placeholder 0.0 does not imply a measured position). Calibration flags reflect records; the separate whole-configuration-valid flag must also pass. Values are commanded, never measured; the last command remains historical after disable/timeout. No current, voltage, torque, measured joint feedback or servo-health values are fabricated. Controller masks indicate mapping only; no I2C probing occurs.

## Sessions, ordering and freshness

HELLO negotiates version 1 through its min/max range, creates a new receiver-issued session/token, and always disarms an existing session. It never enables motion or clears E-stop. The owner supplies a nonzero fresh `session_seed` per endpoint lifetime; each successful handshake consumes the next u64 session number. This must be unique across restarts before physical deployment (generation/persistence/entropy method TBD). Counter exhaustion fails closed. Deterministic test seeds are explicitly synthetic and must not be reused as production boot identities.

The configuration tag is a nonzero, owner-assigned revision of the immutable actuator configuration, not a cryptographic hash or proof of mechanical calibration. It is returned in HELLO_ACK and must be echoed by ENABLE_REQUEST. Assignment and persistence in production remain TBD; a missing tag blocks handshake/enable. Changing configuration requires a new controller/session and revision. The client cannot mutate calibration over this protocol.

Except for HELLO, GET_STATUS, DISABLE and EMERGENCY_STOP, commands require the current session, strictly increasing u32 request IDs and the current token. Sequence wrap is not supported: start a new handshake/client. Syntactically valid commands consume their ID after session/sequence/token checks, even if semantic validation returns NACK. Duplicate/out-of-order requests are NACKed; they are never re-executed and no response cache or automatic retry is implemented. Loss of an ACK leaves command completion uncertain; query status or disable rather than replaying targets.

HELLO and successful HEARTBEAT issue tokens with receiver-monotonic deadlines. ENABLE/target messages require a token younger than `command_timeout_us`. A delayed previously unseen command cannot acquire a new validity window merely by arriving late. HEARTBEAT may renew the current token while its session is alive, even if that token aged out during a disabled idle period. Old tokens are rejected after rotation. Read-only GET_STATUS does not refresh heartbeat, token or motion deadlines or consume the control sequence.

DISABLE and EMERGENCY_STOP are accepted for any correctly framed version-1 empty-payload request, even without a handshake or with an old session/request ID/token. Both are idempotent and invalidate the session. GET_STATUS remains available before handshake and after E-stop. Frames still require valid length and CRC; corrupt stop packets cannot be safely interpreted.

## Timeouts and E-stop

`Settings` requires explicitly provided positive heartbeat, command/freshness and frame-assembly timeouts in microseconds, plus session seed and configuration tag. All default to unset/TBD. None is a production recommendation. The shared host fixture uses 1000/500/100 microseconds respectively solely to advance a fake clock deterministically.

- Startup and constructing an endpoint leave the actuator disabled.
- Missing configuration/clock prevents handshake and enable.
- Only valid current-session HEARTBEAT refreshes heartbeat liveness. Normal commands/status/corrupt packets do not.
- A successful explicit enable or accepted target batch refreshes the motion lease. Heartbeats never extend it.
- At `elapsed >= timeout`, heartbeat loss disables output forwarding and invalidates the session. An expired motion lease does the same while SimulationEnabled. No old motion resumes on reconnect; HELLO plus explicit enable is required.
- Clock unavailability/rollback, transport close/backpressure and partial-frame expiry also disable/invalidate. Status, DISABLE and E-stop remain available without a clock; no handshake or enable is possible until clock availability returns.
- E-stop latches `ActuatorState::Fault`, including in locked firmware. HELLO, heartbeat, enable and targets are then rejected. DISABLE, GET_STATUS and repeated E-stop remain available. `start()` and `disable()` cannot clear the latch. There is no wire reset/recovery message. Reconstructing an offline test process is an explicit new test lifecycle, not an automatic recovery policy. A physical recovery/reset design remains future work.

Physical permission remains false in every message and every build. The existing ESP32 compile guard rejects host-simulation flags; PlatformIO upload guards remain unchanged. No real backend exists. Software disable/E-stop does not cut servo power or stop previously programmed external hardware; verified OE/cutoff wiring and electrical/mechanical calibration remain prerequisites for a later hardware phase. CRC/session tokens are not protection against a malicious authenticated-looking sender; authorization and physical-link selection are still future work.

## Example exchange and tests

Logical example using synthetic fixtures: `HELLO(request=1, versions=1..1)` -> `HELLO_ACK(session=100, token=1, revision=7, physical=0)`; `ENABLE_REQUEST(request=2, session=100, token=1, revision=7)` -> `ACK(state=SimulationEnabled, physical=0)` on a simulation build, or `NACK(PhysicalLocked)` on the locked build. A target command must then pass the installed configuration's limits. `HEARTBEAT` rotates the token; future commands use the new token. `EMERGENCY_STOP` -> ACK with Fault/EmergencyStop and session 0; the next enable is rejected as Stopped.

[golden_vectors.txt](golden_vectors.txt) contains literal wire bytes for HELLO, a target and ACK. Both implementations encode and decode these same vectors; Python also verifies CRC with the independent standard-library `binascii.crc_hqx`. C++ tests exercise every payload length, corrupted/truncated packets, boundary validation, sessions, timeouts, E-stop, queue failure and fixed-seed malformed streams. Python tests exchange real encoded bytes with the actual C++ endpoint via host-only in-memory transport bridges, in both simulation and locked builds.

Run all tests using the CMake/CTest commands in [Firmware/README.md](../Firmware/README.md). Python 3.11+ is now required for the reference-client tests; only the standard library is used. There are no real-link, radio or serial dependencies and no tests energize hardware.
