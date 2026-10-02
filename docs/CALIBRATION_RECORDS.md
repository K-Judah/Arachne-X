# Calibration records and simulated persistence

This phase adds a portable C++17 record codec and persistence boundary in `Firmware/calibration/`. It does not calibrate physical servos. **Physical output remains compiled out, boot remains disabled, and valid records do not enable motion.** Protocol v1, its wire format and the Python reference client are unchanged; there are no remote calibration commands.

## Representation and evidence

`Record` holds a complete `RobotConfiguration`, three externally supplied positive revision IDs, provenance, an optional externally supplied Unix timestamp in seconds, and optional method/approval evidence IDs. Each of the 18 joints retains its logical identity, controller/channel, direction, calibration state, angle minimum/neutral/maximum in radians and pulse minimum/neutral/maximum in microseconds. Pulse points must be retained to restore the existing configuration completely; none is derived from assumed MG996R specifications.

Default construction provides named but unconfigured joints, unset calibration values, zero (unresolved) revisions and `Unapproved` provenance. Incomplete drafts can exist in memory but cannot be serialized or committed by this codec. No production calibration fixture is included.

Three independent questions must pass:

1. **Format/integrity:** expected size, magic, version, reserved fields, metadata encoding and CRC.
2. **Configuration:** the existing validator checks all 18 identities, unique controller/channel pairs, enums, calibration states, finite values, strict min/neutral/max relationships and required fields. `Report` exposes format/configuration flags and the existing bounded joint diagnostics. Format-valid bytes can still contain invalid configuration.
3. **Evidence/policy:** a complete configuration is not proof of physical calibration. `Unapproved` records cannot become committed snapshots through `Repository`. `SyntheticTest` requires explicit `AllowSyntheticForHostTests`; ESP32 compilation rejects synthetic loading regardless of that policy. `PhysicalApprovalClaim` requires positive externally supplied method and approval IDs. These are references to an external review, **not verified evidence, a cryptographic signature, or permission to actuate**. A claimed `Calibrated` flag alone is insufficient.

The default loading policy requires a physical approval claim. The host tests explicitly mark their records synthetic and reuse the existing asymmetric arithmetic fixtures. Tests of approval handling use clearly labelled synthetic evidence IDs; they do not establish robot calibration. There is no automatic conversion of synthetic provenance to physical approval.

## Schema v1: exact binary layout

The record is exactly **998 bytes**: 40-byte header, 18 rows of 53 bytes, and 4-byte CRC. Integers are unsigned little-endian. Numeric calibration values are IEEE-754 binary64, little-endian, matching the existing configuration type. No native C++ structure padding is persisted. Encoding sorts rows by logical joint identity; caller slot order does not affect bytes. Decoding accepts any row order if the complete configuration validates and all identities are unique.

| Offset | Size | Meaning |
| --- | --- | --- |
| 0 | 4 | ASCII `AXCR` |
| 4 | 2 | Schema version, 1 |
| 6 | 2 | Total record length, 998 |
| 8 | 4 | Configuration revision, nonzero |
| 12 | 4 | Geometry revision, nonzero |
| 16 | 4 | Hardware/wiring revision, nonzero |
| 20 | 1 | Timestamp present flag, 0 or 1 |
| 21 | 1 | Provenance: 0 Unapproved, 1 SyntheticTest, 2 PhysicalApprovalClaim |
| 22 | 1 | Joint count, exactly 18 |
| 23 | 1 | Reserved, must be 0 |
| 24 | 8 | Unix timestamp seconds; zero when absent; present zero is distinguishable |
| 32 | 4 | Method/notes identifier, 0 means absent |
| 36 | 4 | Approval evidence identifier, 0 means absent |
| 40 | 954 | 18 joint rows |
| 994 | 4 | CRC-32 over bytes 0 through 993 |

Each joint row has these offsets relative to its start:

| Offset | Size | Meaning |
| --- | --- | --- |
| 0 | 1 | Joint ID 0-17: LF, LM, LR, RF, RM, RR; coxa/femur/tibia within each leg |
| 1 | 1 | Controller: 0 A, 1 B |
| 2 | 1 | Channel: 0-15 (device domain, not a selected harness map) |
| 3 | 1 | Direction: 0 Normal, 1 Inverted |
| 4 | 1 | Calibration state: 0 Uncalibrated, 1 Calibrated; uncalibrated fails configuration validation |
| 5, 13, 21 | 8 each | Minimum, neutral, maximum logical angle, radians |
| 29, 37, 45 | 8 each | Minimum, neutral, maximum pulse, microseconds |

Integrity uses **CRC-32/ISO-HDLC**: polynomial 0x04C11DB7 (reflected implementation 0xEDB88320), initial 0xFFFFFFFF, reflected input/output, final XOR 0xFFFFFFFF, checksum stored little-endian. `123456789` yields 0xCBF43926. CRC covers metadata and every joint field. It detects accidental corruption, not tampering or forged approval. Authentication and trusted evidence management remain future requirements before deployment.

Wrong size, truncation, trailing bytes, bad CRC, unknown schema, unresolved revisions, unknown provenance, noncanonical timestamp flags and nonzero reserved bytes are rejected. Configuration problems are returned as joint diagnostics. An unsupported schema is rejected, never guessed or silently migrated. Encode/decode output arguments remain unchanged on failure. All codec buffers and reports are bounded; no heap allocation is required.

## Lifecycle and activation boundary

`Repository` is single-owner and noncopyable, associated with a `Storage`, expected revisions and the existing `ActuatorSystem` safety interface. It exposes these states:

| State | Meaning |
| --- | --- |
| Absent | No usable snapshot, or explicitly invalidated; no implied default calibration |
| Candidate | A complete record has been staged but is not committed or available as a snapshot |
| Validated | Read-back bytes passed integrity, configuration, revision and provenance checks; still no snapshot |
| Committed | Committed bytes were read and revalidated; an immutable public snapshot is available |
| Rejected | An operation failed; no usable snapshot is exposed |

Normal sequence: `stage(record)` -> `validate_candidate()` -> `commit()`. A commit cannot skip validation. Unapproved data may be staged for inspection but fails the eligibility check. A successful commit publishes a snapshot only after reading and checking committed storage again. `load()` checks committed bytes only; it never promotes an abandoned candidate, even if that candidate was validated before interruption. `interrupted_candidate()` identifies pending bytes from an interrupted/abandoned transaction; incomplete candidate writes also return an error. There is no automatic retry.

Construction, staging, validation, commit, load and invalidation disarm the associated actuator and never send output. Rejection clears the repository snapshot; it does not partially overwrite the actuator's immutable configuration. `disable()` cannot clear an existing actuator fault, so E-stop stays latched throughout these operations.

The only restored configuration is `repository.snapshot()->configuration()` after a successful load/commit. It is const through the public API. An owner may use it to construct an explicit new **disabled** actuator lifecycle; the existing actuator remains unchanged. There is no live configuration swap, auto-enable, reboot hook or fault recovery mechanism. Owners must not reconstruct an actuator to bypass an existing fault. This phase does not wire storage loading into `app_main`; firmware remains inert. Keeping an old snapshot does not establish that its revisions remain suitable after a hardware change.

## Atomic storage contract

`Storage` abstracts staging, candidate reads, `commit(expected_bytes)`, committed reads and pending-candidate presence. It has no NVS, filesystem, flash, SD, Pi storage, GPIO or bus dependency. The contract requires:

- Candidate writes never modify committed bytes, including on interruption.
- Reads return complete bytes or fail; malformed complete data is rejected by the codec.
- Commit succeeds only for the exact candidate that was validated. Publication is all-or-nothing; a failed commit preserves the previous committed record.
- The previous record remains authoritative until the atomic commit boundary. A crash after that boundary can expose the complete new record; a crash before it exposes the old record.
- A future physical adapter must independently prove durability and atomicity, including its synchronization/commit marker and power-loss behavior. The interface alone cannot guarantee real flash behavior.

`MemoryStorage` implements this contract for a single host thread with separate candidate and committed byte buffers. It offers deterministic interrupted-write, failed-commit, failed-read and corruption injection. Data survives repository destruction/recreation while the storage object lives, **not host process exit**. It is guarded against inclusion on ESP32. No physical storage implementation is selected.

Last-known-good recovery means explicitly loading the untouched previous commit after a failed write/commit. No abandoned candidate is activated. If committed bytes themselves are corrupt or mismatch current revisions, loading fails closed; the implementation does not silently revert to a potentially obsolete calibration. The storage simulation does not retain a multi-generation archive.

## Revision changes and remaining TBDs

`Revisions` carries configuration, geometry and hardware/wiring IDs. Values are positive owner-assigned identifiers; zero means unresolved and blocks serialization/loading. This is a software record contract, **not an invented CAD revision naming scheme**. Assignment, mapping to actual CAD/hardware, persistence and evidence provenance remain TBD. The caller must supply expected IDs; the library does not discover physical changes.

Servo/horn replacement, remapping, geometry changes or changed calibration require the responsible owner to assign relevant new revision IDs and call `invalidate(new_expected)`. Invalidation disarms and drops the active snapshot without deleting old committed bytes. Those bytes then fail revision checks. Resubmission requires the complete candidate/validation/commit sequence. Do not reuse IDs for physically different configurations. The configuration revision can identify the matching immutable protocol configuration tag, but no automatic protocol update or new wire command is added here.

Actual wiring, directions, angles, pulse ranges, neutral offsets, hardware approval procedure, evidence authenticity, revision registry, storage hardware, power-loss durability, wear policy, geometry and cutoff design all remain TBD. No numeric robot calibration is supplied. Valid persisted bytes cannot establish mechanical clearance, load capability, wiring correctness or power safety; physical servo output remains unavailable.

## Validation and reproduction

Use the host commands in [Firmware/README.md](../Firmware/README.md). CTest now includes `calibration_tests` and `calibration_locked_tests` alongside the original five suites; the existing CI host job discovers all seven automatically. No additional Python packages are required.

Verified locally on 2026-10-02 from starting commit `4d14f5317edad2339ed38934f25a12bffd339b41`: MSVC 19.51.36260.0 Release build passed with /W4 /WX, and **131/131 cases across seven suites** passed: original 33 control + 4 locked + 29 protocol + 2 protocol-locked + 29 Python/integration, plus 32 calibration + 2 calibration-locked. The initial build caught a signed/unsigned comparison in a new test; it was corrected before the successful run. The ESP32/simulation compile guard and upload guard passed. Existing protocol implementation and all prior tests remain unchanged.

Calibration robustness includes 7,984 individual bit flips, all 998 truncation points, interruption at all 998 candidate-write boundaries, and 2,000 random inputs plus 2,000 checksum-repaired mutations. A checksum-repaired mutation may represent valid alternate data; it must still pass all configuration checks. Tests cover revision/provenance rejection, no partial result, last-good recovery, full replacement, E-stop and physical lock. No hardware was used. ESP32 cross-compilation remains unverified because the SDK/compiler are unavailable; repeated toolchain downloads were not attempted.
