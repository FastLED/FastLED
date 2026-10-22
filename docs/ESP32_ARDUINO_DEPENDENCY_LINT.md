# Native ESP32 Arduino dependency checks

`bash lint` checks both direct Arduino dependencies and the include closure of
native ESP32 implementations. The Rust `EspArduinoHeaderChecker` reports an
include chain such as `native.hpp -> wrapper.h -> FS.h -> Arduino.h`.
Application adapters under `src/platforms/arduino/` may use Arduino interfaces;
native implementations must not reach those adapters implicitly.

## Application include migration

On ESP32 Arduino builds, `FastLED.h` no longer includes `<Arduino.h>`.
Sketches that receive that header from the Arduino builder are unchanged.
Non-`.ino` translation units using Arduino APIs such as `Serial`, `pinMode`,
or `digitalRead` must include `<Arduino.h>` themselves, before using those APIs:

```cpp
#include <Arduino.h>
#include <FastLED.h>
```

This keeps application Arduino interfaces explicit while FastLED's ESP32
implementation uses native SDK headers. Other Arduino platforms retain their
existing include behavior.

## Policy and scope

- Comments and string literals are ignored. Native feature branches, including
  `#if 0`, are inspected so disabled code cannot hide a dependency.
- Branches proven exclusive to foreign architectures are excluded. Unknown
  conditions remain reachable; ESP32 chip variants are not excluded.
- Repository-owned wrappers cannot hide includes behind macro expansion or
  probe for Arduino header availability. Use literal native SDK headers.
- Small C HAL headers are allowed when their reachable dependencies satisfy
  the policy. Installing Arduino headers alone does not select an adapter.
- ESP8266 retains its existing storage/SPI behavior; the transitive pass roots
  are native ESP32 files.

The legacy SPI opt-in remains supported. Only the exact dispatch edge guarded
by `FL_ESP32_SPI_ARDUINO` and `ARDUINO` can reach its compatibility shim. That
shim must contain only its forwarding include and pragma. Its implementation
lives in the application adapter directory; unrelated native wrappers cannot
use this exception.

## Build metadata and limits

The existing whole-project Rust lint pass uses `compile_commands.json` in the
project root and `.build/fbuild/*/`. It inspects every ESP32 entry's `arguments`
array, or shell-word-parsed `command` when the array is absent, and deduplicates
include-path profiles. Quoted includes search their containing directory first;
angle includes use configured search paths.
Compiler include-class precedence (quote, regular, then system paths) and
directory order within each class are preserved. A fbuild-staged
`<environment>/lib/FastLED` path is replaced at that same position by current
`src/`, so lint examines current sources instead of an old staged snapshot.

Without SDK metadata, local source dependencies are still checked and known
Arduino-owned interfaces (`FS.h`, `LittleFS.h`, `SPI.h`, `Wire.h`, `WiFi.h`)
cannot silently pass as unresolved headers. SDK metadata strengthens coverage
for arbitrarily named vendor wrappers. Run a representative fbuild build to
populate it; lint does not download or build a toolchain itself.

This is a bounded structural dependency check, not a full C++ preprocessor.
It follows literal SDK includes, caches parsed files, handles cycles, and
limits external traversal to 4096 reachable headers per profile. Macro
includes inside installed SDKs are not expanded. Malformed shell `command`
strings are skipped. Response-file expansion is not provided by this check.
Arbitrary missing SDK headers cannot establish a verified closure, so
representative SDK compilation remains necessary. Save-time single-file checks retain the direct rule; the closure
runs during whole-project lint.

The full lint command took approximately 56 seconds in this checkout with
many existing SDK build profiles and concurrent builds. Cost depends on the
number of distinct profiles and reachable headers. No new workflow is needed.

## Verification for #4891

Focused tests invoke the production Rust CLI. The original transitive-wrapper
case failed before implementation. Additional RED to GREEN cases cover later
compilation entries, include precedence, wrappers outside `src/`, cycles,
macro includes, availability probes, adapter aliases and inactive branches.
All 26 focused cases and 171 Rust unit tests pass (2 Rust tests ignored).
The full native debug suite passes 326 unit targets and 95 examples.

A selective revert of the original `FastLED.h` Arduino import guard makes
`bash lint --cpp` fail with the real chain
`platforms/esp/compile_test.hpp -> FastLED.h -> fl/system/arduino.h`.
The corrected guard was restored immediately afterward.

The dependency repair also removes the implicit Arduino imports in the ESP32
umbrella headers and the LEDC Arduino fallback. FastPin now explicitly includes
the vendor GPIO register header. SPI adapters use SDK bit-order constants.
AutoResearch uses explicit FastLED timing APIs where it previously inherited
Arduino declarations.

Matched Lolin D32 builds compare `b58cf683f` with this change, using
espressif32 6.13.0 / Arduino 2.0.17 / IDF 4.4.7 and `bash bloat`:

| Configuration | Flash before / after | Static RAM before / after |
|---|---:|---:|
| No SPI | 231094 / 231094 | 21000 / 21000 |
| Legacy APA102, explicit native HOST2 | 304154 / 304154 | 23040 / 23040 |

Attributed symbols also match. This change enforces the dependency boundary;
these workloads show no flash or static RAM saving. Equal-length staging
paths avoid diagnostic filename strings distorting the comparison.

Real SDK probes pass the explicit legacy SPI opt-in and standalone Arduino
SPI adapter on classic ESP32 / IDF 4.4 and P4 / IDF 5.5.5. Canonical
AutoResearch compiles and links on S3 / IDF 5.5.5 with the native USB
Serial/JTAG backend. These compilation results do not establish physical USB
enumeration, reconnect or ownership behavior; #4893 tracks that evidence.
Physical classic ESP32 / IDF 4.4 canonical AutoResearch passes three legacy
RMT scenarios, including both payload captures with 4/4 checks. The canonical
image remains installed after verification.
