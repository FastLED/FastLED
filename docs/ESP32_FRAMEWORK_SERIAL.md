# Optional Arduino UART startup

Issue: [#4892](https://github.com/FastLED/FastLED/issues/4892).
Framework patch: [espressif/arduino-esp32#13010](https://github.com/espressif/arduino-esp32/pull/13010).

Arduino 3.3.11 calls `Serial0.setPins()` from framework startup before application
`setup()`. That strong reference extracts HardwareSerial and its global objects
even when the application uses only FastLED's native console.

The framework patch replaces that reference with a checked, weak C hook. The
strong provider lives with `Serial0` in HardwareSerial.cpp and performs the same
pin setup. An application's normal HardwareSerial reference extracts the provider;
the startup weak reference alone does not extract that archive member. Existing
Arduino APIs and global instance guards remain intact.

## Integration

`ci/boards.py` pins the immutable patched Arduino 3.3.11 source commit together
with the matching pioarduino IDF 5.5.5 platform for esp32dev, esp32s3 and esp32p4.
The P4 profile moves from IDF 5.5.1 to 5.5.5. Legacy IDF 4.4 profiles retain their
existing framework. This is a downstream framework pin pending upstream adoption.
The pristine comparison commit is `189089bb76e74978dc95abebefdad42b0d421ba9`;
the patched commit is `8ec18a5aee996356e906a759dbcc1e12b46fb0aa`. Both archives
come from the same public fork URL pattern used in `ci/boards.py`. To repeat a
matched comparison, change only that framework commit in an isolated checkout,
keep IDF 5.5.5 and application flags fixed, rebuild, and run `bash bloat` for each
image. Do not compare the old P4 IDF 5.5.1 profile against the new profile to
attribute the hook's size effect. Retire the fork pin when an upstream package
containing this hook passes the same link and hardware checks.

Normal `bash compile esp32p4 --examples Blink` and
`bash compile esp32s3 --examples Blink` builds pass with the production pins.

## Matched P4 link experiment

Both sides use Arduino 3.3.11 and IDF 5.5.5; only the startup hook changes.
These are small controlled applications, not the full AutoResearch image.

The following totals are fbuild's build summary measurements.

| Application | Flash before | Flash after | Static RAM before | Static RAM after |
|---|---:|---:|---:|---:|
| Native UART, no application Serial | 337,610 | 315,570 | 24,592 | 23,496 |
| Native USB CDC, no application Serial | 380,686 | 352,268 | 73,016 | 71,888 |
| Explicit application Serial0 | 345,102 | 345,128 | 24,592 | 24,592 |

`bash bloat` against the same six public-source-pin ELFs reports the following
symbol attribution totals. These differ from build-summary region accounting and
must not be mixed with it. Firmware binary padding is another separate metric.

| Application | Attributed flash before | After | Attributed RAM before | After |
|---|---:|---:|---:|---:|
| Native UART | 330,264 | 308,233 | 25,609 | 24,532 |
| Native USB CDC | 372,727 | 344,416 | 73,591 | 72,473 |
| Explicit Serial0 | 337,796 | 337,822 | 25,609 | 25,609 |

The analyzer's image-flash measurements are respectively 346,350 → 324,300,
389,390 → 361,026 and 353,938 → 353,964 bytes. All three measurements confirm
substantial savings for native-only applications and the small hook cost for
explicit Serial0 use. The symbol analyzer's host objdump cannot disassemble this
RISC-V ELF; symbol sizes and map attribution are available, but disassembly-based
reference analysis is not established by these reports.

The no-Serial controls omit HardwareSerial and Serial0. This does not imply every
String or Stream owner disappears: chip debug reporting retains String, and
AutoResearch OTA's explicit filesystem integration can retain Stream independently.
P4 compile/link evidence does not establish physical USB behavior or runtime heap.

## Hardware validation command

A separate matched classic ESP32 AutoResearch native-only experiment used the
pristine and patched public source archives with IDF 5.5.5. Free 8-bit heap was
71,512 bytes before and 72,124 after: 612 bytes recovered. All five cold/settling/
warm probes on each image passed heap integrity and retained zero heap per cycle.
The larger AutoResearch workload measured 1,729,223 → 1,717,751 build-summary
flash bytes and 75,224 → 74,856 static RAM bytes. These classic-board results are
separate from the small P4 control table above.

The matched explicit Serial0 AutoResearch controls measure 1,736,235 → 1,736,263
build-summary flash bytes and unchanged 75,240-byte static RAM. Their attributed
flash is 1,603,262 → 1,603,288 bytes and attributed RAM remains 149,526 bytes.
For native-only AutoResearch, attributed flash is 1,596,574 → 1,585,515 and
attributed RAM is 149,259 → 148,466 bytes. Differences between accounting methods
are preserved rather than presented as interchangeable totals.
Both explicit Serial0 images report 71,468 bytes free heap in every cold,
settling and warm probe, with zero per-cycle retention and valid heap integrity.
The patch therefore recovers native-only heap without increasing measured
runtime heap use for the explicit Serial0 workload.

On a classic ESP32 with GPIO4 available for the canonical loopback fixture:

```sh
bash autoresearch esp32dev --serial-startup native-first --upload-port /dev/ttyUSB0
bash autoresearch esp32dev --serial-startup arduino-first --upload-port /dev/ttyUSB0
bash autoresearch esp32dev --serial-startup native-only --upload-port /dev/ttyUSB0
```

The application modes test default UART pins, temporary GPIO16/17 selection,
restoration, both initialization orders, alternating complete native/Arduino
records and warm-cycle heap. GPIO16/17 must be available for these transitions.
The native-only fixture checks that the linked image excludes HardwareSerial.
The runner checks framework/SDK witnesses and the analysed ELF before deployment.
Sequential output checks do not establish concurrent writer atomicity. Physical
native USB ownership and reconnect acceptance remains tracked by #4893.
