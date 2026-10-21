# Remaining ESP32 flash increase after #4871

## Status

[PR #4871](https://github.com/FastLED/FastLED/pull/4871) merged as
`0c088a17fd1e056af5081581ef5798330088e338` after all 122 hosted checks passed.
Its merge tree matches the tested head. The optimized real KissFFT path remains
unchanged; the experimental ESP-DSP adapter is removed.

The matched Lolin D32 sketch still measures 272,434 bytes board flash and
23,292 bytes static RAM, versus 253,022 / 22,216 for 3.10.3. **The remaining
19,412-byte flash increase has not been removed.**

## Important correction: these images select different LED backends

The pinned Arduino ESP32 2.0.17 / IDF 4.4.7 SDK describes eight RMT channels
and 64-word blocks in `soc/soc_caps.h`, but does not define
`SOC_RMT_SUPPORTED`. FastLED's `feature_flags/enabled.h` defaults the missing
macro to zero. The Arduino 2.0.3 legacy Blink budget has the same problem.

The actual ELF symbol inventories confirm the result:

| Image | Selected implementation symbols |
| --- | --- |
| 3.10.3 | `ESP32RMTController` (14 distinct names), no generic fallback |
| Merged FFT fix | `ClocklessBlockingGeneric<12, ...>` (six distinct names), no `ChannelEngineRMT4` |

Therefore, the earlier size table is a valid observation of the same sketch
and toolchain across versions, but **not evidence of equal RMT runtime behavior**.
The before/after FFT-removal controls both used the fallback, so the measured
240,676-byte flash and 7,052-byte RAM savings remain real. The remaining gap
must not be presented as the necessary cost of robust RMT.

Espressif's [legacy RMT documentation](https://docs.espressif.com/projects/esp-idf/en/release-v4.4/esp32/api-reference/peripherals/rmt.html)
also confirms the classic ESP32's eight-channel peripheral and shared
512-word symbol memory. Hardware absence is not the explanation.

## Where the remaining image growth appears

Comparing allocated flash/IRAM section contributions from the two matched
linker reports, rather than adding symbol aliases, shows these net archive
changes:

| Archive | Section-byte change |
| --- | ---: |
| FastLED | +11,890 |
| Arduino core | +5,097 |
| SDK peripheral drivers | +2,901 |
| SDK heap implementation | +1,768 |
| SDK ring buffer | -3,593 |

These are attribution clues, not an exact partition of the 19,412-byte image
delta: loadable data, other archives and alignment also contribute.

Concrete roots include:

- `platforms+` references `HardwareSerial::available()` and Arduino `String`
  operations even though the sketch does not use Serial. UART code and the
  Serial object's virtual functions remain linked.
- GPIO, RTC I/O and timer dependencies are larger. The reporter explicitly
  uses the power-indicator API, so removing all GPIO support is not valid.
- New frame-power planning, dithering power reserves, strings, allocation
  helpers and scheduling contribute FastLED code. The power planner's two
  largest new named routines alone account for roughly 2 KB; they implement
  behavior absent from the old simple limiter and cannot just be discarded.

The source-boundary/Serial reachability deserves a smaller follow-up experiment.
It must preserve explicit Serial initialization, readiness, input, output and
diagnostics. A blanket weak-link or log-disable change is not established safe.

## Rejected experiment: native IDF console

Built the same fallback sketch with the existing
`FL_ESP_USE_IDF_SERIAL=1` selection, explicitly keeping
`FASTLED_ESP32_HAS_RMT=0` so only the console choice changed:

| Console | Board flash bytes | Static RAM bytes |
| --- | ---: | ---: |
| Existing Arduino console | 272,434 | 23,292 |
| Native IDF console | 277,366 | 23,052 |

That **adds 4,932 bytes flash**, while saving 240 bytes static RAM. It is not
adopted. No diagnostics, networking, FFT features or RMT protections were
disabled to obtain a smaller published figure.

## Real RMT probe: correctness work precedes a new size claim

Forcing the RMT capability on exposed dormant TX/RX interface drift. Local
experimental fixes corrected missing overrides, an obsolete decode-error name,
optional resolution handling and a stale shared-pointer construction pattern.
A capability-selection test failed the two legacy cases before the detection
fix and passed all eight cases afterward; existing reset-timing tests also
passed. These probes are not production changes.

The actual SDK build then reached the linker and failed with IRAM literal
relocations in `tx_start`, `onTxDoneInterrupt` and `handleInterrupt`. No valid
real-RMT image size or waveform result is claimed.

Two additional runtime risks were found by source review:

1. The engine installs a custom non-shared interrupt handler, then calls
   `rmt_driver_install`, which installs the system handler. The SDK documents
   custom and system ISR ownership as mutually exclusive; 3.10.3 kept them
   separate.
2. New TX channel IDs advance by one while each transmission reserves two
   hardware memory blocks. Adjacent active channels can overlap; the old
   implementation advanced by the block count. Multi-strip allocation needs
   explicit validation.

The experimental detection/compile patch is preserved locally at
`.build/4870-evidence/legacy-rmt-investigation.patch` and passes
`git apply --check`. It is **not activated or included in this documentation
change**. Enabling it by default would not meet the requirement to preserve
transmission quality.

## Next work

1. Repair and validate legacy RMT selection, IRAM placement, interrupt ownership
   and non-overlapping multi-strip blocks, retaining timing and recovery behavior.
2. Re-establish a real-RMT versus real-RMT size control on the same SDK.
3. Measure a focused Serial/provider isolation change and retain it only if
   actual image size improves and Serial/diagnostic behavior is preserved.

The linker reports, build logs and experimental scripts are retained under
`.build/4870-evidence/`, `.build/symbols/4870-lolin-*` and
`.build/4870_measure.py`. No production runtime changes or new workflows were
made during this follow-up investigation.


## Native IDF console optimization

The earlier native-console measurement retained generic UART configuration
branches even though `EspIO` always selects `UartConfig::reliable(UART0)`.
A private console constructor now passes that fixed configuration through the
same inline initialization helpers as the public configurable constructor.
The compiler can discard impossible pin-routing, inversion and conversion
branches from console-only images. The public configured UART constructor and
its runtime options remain available.

Matched unchanged reporter sketch, Lolin D32, Arduino ESP32 2.0.17 / IDF 4.4.7 /
GCC 8.4 patch5, controls pinned to `f1efdf028`, with native Serial selected by
`FL_ESP_USE_IDF_SERIAL=1`:

| Console | Board flash bytes | Physical static RAM bytes |
| --- | ---: | ---: |
| Arduino control | 272,434 | 23,292 |
| Native IDF before | 277,366 | 23,052 |
| Native IDF specialized | 274,842 | 23,052 |

The native image saves **2,524 bytes flash** with unchanged physical static
RAM. It remains **2,408 bytes larger** than the Arduino control. Default console
selection is unchanged. These figures do not include the separate #4873 cleanup;
its savings must not be added without measuring the combined image.

The map-section changes are chiefly UART driver (-1,116 bytes), GPIO driver
(-865), FastLED platform code (-343) and UART HAL (-250), with small opposing
alignment/other changes. `uart_set_pin` and `uart_set_line_inverse` disappear
from the console-only ELF, while a real SDK consumer using runtime UART1 baud,
pins, inversion and an event queue still retains both functions and the public
configured constructor. SDK ring-buffer creation and UART installation remain:
those support the existing buffered console behavior.

The optimization preserves configuration values, 256-byte RX/TX buffers,
requested LEVEL3/IRAM flags, existing-driver detection, installation delay,
ROM fallback, diagnostics and USB-JTAG selection. Actual ISR placement remains
controlled by the SDK build, as before; the SDK normalizes the IRAM flag to
`CONFIG_UART_ISR_IN_IRAM`.

### Hardware validation and harness corrections

The connected CP2104 device at `/dev/ttyUSB0` (USB serial `0254E5B6`) probes as
classic ESP32. On Arduino ESP32 2.0.3 / IDF 4.4.1, pin-free AutoResearch RPC
smoke passes with **both Arduino and optimized native consoles**: discovery
(83 methods), help, repeated ping, nested payload echo, status, task-context
response and structured missing-method errors. This validates console transport;
it is not an RS485, GPIO loopback or flash-cache stress test.

Two existing harness problems were found and corrected during that validation:

- The edge probe used a cycle-counter API absent in IDF 4.4. It now uses Arduino's
  SDK-compatible cycle-counter wrapper, preserving the cycle arithmetic.
- RPC input honored the selected console, while responses bypassed it through
  Arduino `Serial`. Both original and optimized native firmware failed the RPC
  probe before this correction. Raw responses now use `fl::write_bytes`, keeping
  CRLF framing, streamed byte lengths, flush placement and log-filter bypass.

Deployment remains through fbuild. Its native flashing transport timed out and
held the port during fallback; the supported settings
`FBUILD_USE_ESPFLASH_VERIFY=0 FBUILD_USE_ESPFLASH_WRITE=0` enabled fbuild's alternate
transport and both completed tests. The temporary native test define in
`ci/boards.py` was restored; no board defaults or new workflows are committed.

Final validation also passes 321 native debug unit tests and 95 examples,
repository lint, modern ESP32 native-console compilation, and ESP32-S3
AutoResearch compilation with the native USB-JTAG console. A clean native test
build resolved a stale cached-artifact timestamp rejection before test execution.
Read-only review found no blocking issues.

Evidence: `.build/4870-evidence/native-*`,
`.build/symbols/4870-serial-native-baseline/`,
`.build/symbols/4870-native-candidate/` and
`.build/symbols/4870-native-feature/`.

## Follow-up: restore the legacy RMT backend

The implementation on `fix/esp32-legacy-rmt-selection` repairs the missing
capability detection and dormant driver errors described above. IDF 4.x channel
geometry now supplies the missing `SOC_RMT_SUPPORTED` default, without overriding
an explicit SDK capability value. Regression tests fail the old ESP32/S2 cases
and pass with the normalization.

TX uses the SDK byte translator and shared RX/TX interrupt handler, preserving
asynchronous operation, chipset timing conversion, latch/reset waits, timeout
recovery and the late-refill check. Physical span reservations prevent TX/TX
and TX/RX memory overlaps. An internal-RAM source passes directly to the SDK;
PSRAM data receives a checked internal-RAM copy retained through completion.

Hardware testing also exposed and repaired RX and harness problems:

- Legacy cycle-counter API compatibility and same-pin GPIO pre-test setup.
- GPIO direction helpers disconnecting the RMT output matrix; vendor enable-bit
  operations now preserve routing, and release preserves idle LOW.
- An oversized receive-buffer request exhausting heap. SDK RX storage is now
  bounded to hardware capture capacity, including ring-buffer header space.
- Empty SDK packets not being returned to the ring buffer. Rearming now stops
  and clears stale captures; complete packets stop RX.
- The decoder assuming HIGH/LOW symbol pairing while classic RX captures
  LOW/HIGH. A directly tested SDK-independent helper handles either phase and
  preserves the last bit when the SDK omits its final LOW duration. Physical
  overflow is rejected before allowing that completed-packet behavior; injected
  edge data remains supported.

### On-device evidence

The connected CP2104 device `/dev/ttyUSB0`, serial `0254E5B6`, identifies as
ESP32-D0WDQ6 revision 1.0. Using Arduino 2.0.3 / IDF 4.4.1 and canonical
`bash autoresearch`, both Channel API (GPIO25) and legacy `addLeds` (GPIO4)
same-pin loopback pass all tested patterns for one- and two-LED strips with
three frames per pattern. No external jumper is required for this same-pin
RMT capture. This proves actual byte transport, not merely successful boot.

A separate existing `timingDriftTest` RPC on the deployed AutoResearch firmware
completes 418 shows of a 35-LED strip, exercising SDK refill beyond its initial
hardware buffer, followed by a successful ping and no watchdog reset. This is
functional progress evidence, not full byte capture of the longer strip or
timing parity: show durations were 2,968–3,045 microseconds and sequence periods
1,347 / 2,580 milliseconds (the first is partial). Wi-Fi and flash-cache stress
remain unmeasured. Classic RX is limited to one hardware memory block here.

### Revised size baseline

Same reporter sketch and Lolin D32 / Arduino 2.0.17 / IDF 4.4.7 / GCC 8.4 patch5:

| Build | Board flash bytes | Physical static RAM bytes |
| --- | ---: | ---: |
| 3.10.3 RMT | 253,022 | 22,216 |
| master `e19e9e7a9`, blocking fallback | 272,434 | 23,292 |
| RMT restoration candidate | 295,714 | 23,844 |
| Candidate minus master fallback | +23,280 | +552 |
| Candidate minus 3.10.3 RMT | +42,692 | +1,628 |

The new ELF contains `ChannelEngineRMT4Impl`, `rmt_write_sample` and
`rmt_translator_init`, with no `ClocklessBlockingGeneric<12...>` controller.
Allocated image flash is 295,713 bytes; board flash is one byte larger, as in
the controls. Attributed RAM from the symbol report is not physical static RAM.

This restores expected hardware behavior; it does not remove the memory
regression. SDK translation also allocates a bounded 512-byte heap buffer for
each two-block classic TX channel, plus bookkeeping, and a raw-byte internal
copy when the source is in PSRAM. Those dynamic allocations are not included
in the static-RAM column. Pending #4873/#4874 savings are not included or added.
Use this working RMT baseline for subsequent size work. Investigate default
Arduino console and managed-power/driver-registration reachability without
removing diagnostics, power safeguards or runtime API behavior.

### Validation and remaining limits

- Full native debug run: 321 unit targets and 95 examples pass.
- New production decoder target: nine direct-helper cases, including real
  LOW/HIGH phase, final-bit preservation, invalid timing and output overflow.
- Focused Python regression contracts: ten cases pass.
- Classic IDF 4.4.1 AutoResearch and IDF 4.4.7 reporter build and run as above.
- Modern IDF 5.5.5 ESP32-S3 Blink builds. Legacy IDF 4.4.7 S3 Blink also builds.
- Legacy S2 source compatibility needed its SDK's 13-bit ADC enum. Final S2
  linking is blocked by fbuild requesting absent `esp32s2.rom.wdt.ld`; C3 is
  blocked by modern ISA/C++ flags passed to the legacy GCC8 toolchain. These
  builds are not claimed as passing. Full hosted validation is required before
  a platform-sensitive merge.
- The capability audit found another confirmed false-negative, WiFi, tracked
  in [#4876](https://github.com/FastLED/FastLED/issues/4876). It is separate work.

Evidence is retained under `.build/4870-evidence/rmt-restored-*` and
`.build/symbols/4870-native-arduino/`. Temporary ROM-print diagnostics were
restored after the controlled experiment. All deployment used fbuild; its
supported alternate flashing transport was selected with
`FBUILD_USE_ESPFLASH_VERIFY=0 FBUILD_USE_ESPFLASH_WRITE=0`. No new workflows.
