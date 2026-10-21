# Remaining ESP32 flash increase after #4871

## Status

[PR #4871](https://github.com/FastLED/FastLED/pull/4871) merged as
`0c088a17fd1e056af5081581ef5798330088e338` after all 122 hosted checks passed.
Its merge tree matches the tested head. The optimized real KissFFT path remains
unchanged; the experimental ESP-DSP adapter is removed.

The FFT-only fix measured 272,434 bytes board flash and 23,292 bytes static
RAM on the matched Lolin D32 sketch. The implementation below reduces that to
**270,414 / 23,148**, saving another **2,020 bytes flash and 144 bytes static RAM**.
Against 3.10.3's 253,022 / 22,216, **17,392 bytes flash and 932 bytes static RAM
remain**. The complete regression has not been removed.

## Implemented dead-dependency cleanup

All measurements use the unchanged reporter sketch and pinned toolchain below;
final results come from a clean rebuild, since an incremental build retained
stale operation-table guards in two objects.

- Ordinary digital pin cleanup used to reference timer interrupt teardown even
  when software PWM was never configured. Successful PWM setup now stores the
  same teardown function in its existing lazy state. Cleanup invokes it only
  for an active ISR. This removes unused timer and GPIO interrupt cleanup from
  pin-only images, while explicit PWM consumers still link the original driver.
  PWM users pay one additional function pointer (four bytes on ESP32); the
  pin-only sketch does not instantiate that state.
- Vector operation tables now contain constant function pointers, using named
  thunks and C++11-compatible constexpr selectors. Copy, move, construction,
  destruction and swap retain their previous implementations and availability.
  Unused initialization guards and runtime table setup disappear.
- UCS7604 current-control constructors are constexpr, eliminating a startup
  initializer and unused default storage from non-UCS7604 images. Runtime
  brightness setters and four-bit masking are preserved.

The final reporter ELF contains no `timer_deinit`, `timer_pause`,
`timer_disable_intr`, `timer_isr_callback_remove`, `gpio_isr_handler_remove`,
vector operation-table guards or UCS7604 startup constructor. A real SDK
consumer explicitly using software PWM setup/cleanup/restart, Serial output
and UCS7604 brightness compiles and links; its ELF retains timer teardown,
Serial initialization and the brightness accessor. This is link evidence,
not an on-device waveform test. Existing native PWM lifecycle and vector
operation tests cover the unchanged runtime paths.

Logging levels, console backend, FFT implementation and selected LED driver
are unchanged. No new workflow or opt-in feature restriction is introduced.
The Blink budget is tightened to reject the preceding FFT-only image on both
flash and attributed RAM; it continues to qualify the generic fallback backend.
Clean Blink board flash/static RAM are **232,250 / 17,248 -> 230,626 / 17,192**;
bloat image flash/attributed RAM are **232,265 / 21,511 -> 230,641 / 21,369**.
The new ceilings are 231,000 image flash and 21,400 attributed RAM. The saved
pre-change report fails both ceilings; the new report passes both.

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
`git apply --check`. It is **not activated or included in this implementation**. Enabling it by default would not meet the requirement to preserve
transmission quality.

## Next work

1. Repair and validate legacy RMT selection, IRAM placement, interrupt ownership
   and non-overlapping multi-strip blocks, retaining timing and recovery behavior.
2. Re-establish a real-RMT versus real-RMT size control on the same SDK.
3. Measure a focused Serial/provider isolation change and retain it only if
   actual image size improves and Serial/diagnostic behavior is preserved.

The linker reports, build logs and experimental scripts are retained under
`.build/4870-evidence/`, `.build/symbols/4870-lolin-*` and
`.build/4870_measure.py`. The initial investigation left production behavior unchanged; the focused
dead-dependency implementation is described above. No new workflows were added.
