# ESP8266 generic software SPI throughput

Investigation of #4966 measures the original GPIO4/16 pair and the subsequently
authorized current wiring, GPIO4/14. All payload transfers use
`GenericSoftwareSPIOutput`; native HSPI remains unchanged.

## Current wiring: normal GPIO bank

Three bidirectional pin scans confirmed GPIO4↔2, GPIO14↔5 and GPIO12↔13.
The target uses GPIO4 for data and GPIO14 for clock; external senses are GPIO2
and GPIO5. GPIO12→13 checks the final level of an unrelated output.

The same-port bulk writer now combines the preceding falling edge with the
next data value, then raises the clock: two stores per bit plus one final
clock-low store per nonempty stream. The old combined-port writer used three
stores per bit. This applies to byte blocks, repeated bytes and pixel blocks,
including optional pixel start bits. Public individual byte writes retain
their atomic data/clock sequence. Explicit atomic mode and ESP8266
separate-port pairs use the atomic single-byte path for block writes.

The ESP8266 policy avoids optional phase padding when a conservative measured
normal-GPIO store floor (six 80 MHz APB cycles) already covers the requested
half-period and 35 ns minimum pulse. GPIO16 is excluded. Longer delays use
a bounded unsigned CCOUNT wait rather than expanding thousands of inline NOPs;
this also prevents flash overflow in the 50 kHz capture fixture.

Measured on ESP8266EX, Arduino core 3.1.2, Wi-Fi disabled, 512 bytes, minimum
of 60 frames, with the highest compile-time rate request:

| CPU | Earlier bulk writer | Optimized bulk writer | Bare two-store calibration |
| --- | ---: | ---: | ---: |
| 80 MHz | 3.10 Mbit/s | 5.36 Mbit/s | 5.71 Mbit/s |
| 160 MHz | 4.42 Mbit/s | 6.66 Mbit/s | 6.67 Mbit/s |

At 160 MHz, lengths 32, 64, 128, 256 and 512 take 6306, 12450, 24738,
49314 and 98466 cycles. The sustained slope is exactly 192 cycles/byte
(24 cycles/bit), matching the bare two-store calibration, with a fixed API
overhead. At 80 MHz, the variable-data writer remains below the constant-data
calibration; no claim of reaching that CPU setting’s absolute floor is made.

All 41 physical 50 kHz captures pass: varied and single-bit patterns, empty
blocks, short tails, repeated values, RGB pixels, pixel start bits and public
byte calls. Each checks the expected bit sequence and exact edge count, final
clock-low state and the final high level on GPIO12→13. This does not detect transient changes
on that unrelated pin. Slow capture proves ordering
and API boundaries; it does not establish full-speed electrical margins.

The fixture changes CPU frequency temporarily and restores it. Its rate
templates remain compiled for F_CPU=80 MHz, so the runtime 160 MHz comparison
measures maximum throughput at a deliberately unattainable 40 MHz request;
it does not establish requested-rate compliance at other CPU settings.

## Original GPIO4/16: RTC clock limit

GPIO16 is part of the RTC block, rather than the normal GPIO bank
([Espressif Technical Reference, section 2.2.5](https://documentation.espressif.com/esp8266-technical_reference_en.pdf)).
The installed Arduino core maps its output through `GP16O`, while GPIO4 uses
`GPOS`/`GPOC`. A bit needs two ordered RTC writes for its clock edges.

On the attached ESP8266EX at 80 MHz, an unrolled GPIO16-only calibration
sustains 2,500,000 register writes/second: approximately 32 CPU cycles per
write. Two writes per clock imply a measured store-throughput bound of
1,250,000 bits/second for this pair. This is a bound derived from this device's
ordered register-write measurements, not a published universal GPIO rating.

The variable-data block writer and a constant-data calibration performing
only the necessary three GPIO stores take the same time for 512 bytes:
262,158 CPU cycles. The ideal 64-cycle-per-bit count is 262,144 cycles, so
the measured block reaches that bound within 0.02% measurement variation. Data preparation fits
inside the RTC-write cost; increasing `DATA_RATE_MHZ` cannot remove that cost.

## Changes and matched measurements

The public byte writer now selects the ESP8266 set/clear register without a
data-dependent branch. Generic block writes on separate ports use that same
atomic path; ordinary same-port pairs use the two-store streaming path described above.
GPIO16 data writes use its RTC register separately. Pin-number mapping still
comes from `FastPin::kRealPin`.

Disassembly also exposed three calls to out-of-line `fl::delaycycles` per
bit. The ESP8266 pin policy now calls the existing inline implementation
from the delay dispatch header. The NOP counts and volatile-store barriers
are retained. GCC documents why its Xtensa volatile serialization inserts
`MEMW` ([Xtensa options](https://gcc.gnu.org/onlinedocs/gcc-10.1.0/gcc/Xtensa-Options.html)).

Matched stock/modified firmware on the same ESP8266EX, Wi-Fi disabled:

| Measurement | Stock | Modified |
| --- | ---: | ---: |
| 64 APA102 LEDs, minimum of 50 frames | 1790 us | 1771 us |
| 512-byte generic block, 6 MHz request | 262260 cycles | 262158 cycles |
| Bare three-store calibration, 512 bytes | 262129 cycles | 262158 cycles |
| GPIO16-only calibration, 4096 writes | 131057 cycles | 131072 cycles |

Calibration's small fixed offsets depend on instruction alignment and timer
placement. Compare the sustained slope and the same-firmware calibration;
do not interpret a few cycles below the fitted bound as a faster physical
clock. The original block writer was already near this RTC limit. The
APA102 improvement is modest (19 us, approximately 1.1%), rather than a
multi-fold speedup. HSPI control at the default request reports 5,714,285 Hz
and a successful 634 us minimum frame; its implementation is untouched.

Running the same fixture with a temporary 160 MHz CPU setting gives
524238 cycles for the variable-data block and 524220 for the bare loop,
again approximately 1.25 Mbit/s. GPIO16-only calibration takes 262014 cycles
for 4096 writes. Doubling CPU frequency doubles cycles per RTC write, so it
does not double this pin pair's wire rate. The fixture restores the previous
CPU frequency before returning.

## Reproduction

```bash
FBUILD_DEV_MODE=1 bash autoresearch esp8266 --upload-port /dev/ttyUSB0 --timeout 15m --skip-lint
```

Use the low-memory JSON-RPC transport, with the positional argument array
nested in `params`, as in `{"params":[[512,30,0,6]]}`:

- `wifiOff []`
- `apa102ShowBitbang [64,50]`
- `bitbangStream [512,30,0,6]`: generic block writer
- `bitbangStream [512,30,1,6]`: individual public byte calls
- `bitbangStream [512,30,2,6]`: constant-data three-store calibration
- `bitbangStream [512,30,3,6]`: GPIO16 writes only; this is not SPI payload
- `bitbangStream [512,30,0,6,160]`: repeat at 160 MHz, then restore CPU frequency
- `bitbangLoopback [-1,5]`: varied 96-byte capture at 50 kHz

Current-wiring commands (the final argument selects GPIO14):

- `bitbangStream [512,60,0,40,80,14]`: optimized bulk API at CPU80
- `bitbangStream [512,60,0,40,160,14]`: optimized bulk API at CPU160
- `bitbangStream [512,60,4,40,160,14]`: bare two-store calibration
- `bitbangLoopback [-1,5,14,96,0]`: varied bytes, external data sense GPIO2
- `bitbangLoopback [170,5,14,96,1]`: repeated raw value
- `bitbangLoopback [-1,5,14,96,2]`: RGB pixel block
- `bitbangLoopback [-1,5,14,90,3]`: RGB pixels with start bits
- `bitbangLoopback [-1,5,14,96,4]`: individual public byte calls

For the original GPIO4/16 pair, physical byte capture remains unverified:
GPIO16 has no detected jumper. Its timing results above must not be confused
with the successful GPIO4/14 captures. Repository and hosted validation are
tracked in `agents/tasks/todo.md` and PR #5021.
