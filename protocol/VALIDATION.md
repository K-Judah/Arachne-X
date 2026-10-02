# Protocol phase validation (2026-10-02)

Starting commit: 7d2f00e. Tests use simulated endpoints only; physical permission is false.

## Executed locally

Windows x64, MSVC 19.51.36260.0, CMake 4.3.1-msvc1; CMake selected Python 3.14.0 for the Python suite. Release compilation passed with project warnings treated as errors (/W4 /WX), with no compiler warnings/errors. Commands from the repository root (using the installed CMake executable's full path because it is not on PATH):

```sh
cmake -S Firmware -B Firmware/build-host -DARACHNE_HOST_TESTS=ON
cmake --build Firmware/build-host --config Release --parallel
ctest --test-dir Firmware/build-host -C Release --output-on-failure
```

CTest: **5/5 suites passed, 97/97 named cases**:

| Suite | Cases passed |
| --- | --- |
| Existing control_tests | 33/33 |
| Existing locked_tests | 4/4 |
| New protocol_tests | 29/29 |
| New protocol_locked_tests | 2/2 |
| Python codec/client + actual C++ endpoint integration | 29/29 |

The robustness cases include every payload length 0-256 with zero/nonzero patterns, 10,000 fixed-seed random streams per C++/Python parser, all single-bit corruptions of an enabled-session target frame in C++, and 100 random streams through the Python/C++ process bridge. Shared literal vectors pass both codecs; Python checks their CRC using independent stdlib binascii.

Verified: disabled startup; no implicit enable; locked-build enable rejection; physical permission always false; full-batch preflight rejection; malformed packets produce no actuator writes; replay/stale-token rejection; heartbeat and command expiry; frame truncation/expiry; closed/full transports; explicit E-stop persistence across start/disable/HELLO; no automatic motion recovery. Existing ESP32/simulation compile-conflict checks pass, and the Python suite executes five isolated upload-guard checks without running an upload tool. The original upload script, PlatformIO configuration, fake hardware and ESP32 entry-point source are unchanged.

## Limits

The SDK and Xtensa compiler remain absent from the local PlatformIO packages. The previous phase documented impractically slow downloading. Per this task's instruction, the download/cross-build was not repeated. **ESP32 cross-compilation remains unverified.** The registered protocol component was compiled in both locked and simulation host variants. The committed GitHub Actions workflow runs the full host suites on Linux/Windows and attempts a compile-only ESP32 build, but GitHub Actions was not executed in this local session.

No physical tests, serial ports, GPIO, I2C or servo output were used. No production timing, mechanical dimensions, addresses, pins, pulse ranges, joint limits or calibration were established. CRC/session freshness does not provide authentication; external clock scheduling, unique boot namespaces, configuration revision management and hardware stop/cutoff remain deployment prerequisites. See [the specification](PROTOCOL.md).
