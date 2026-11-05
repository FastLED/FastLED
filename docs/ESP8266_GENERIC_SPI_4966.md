# ESP8266 generic software SPI on GPIO4/16

Investigation of #4966 keeps GPIO4 as data and GPIO16 as clock. All reported
transfers use `GenericSoftwareSPIOutput`; native HSPI remains unchanged.

## What limits this pair

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
atomic path; ordinary same-port pairs retain their combined two-store path.
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

## Reproduction and remaining verification

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

For loopback, connect GPIO16 (D0) to GPIO5 (D1). The ISR reads data directly
from GPIO4's input register. Constant patterns 0, 255, 170 and 85 can replace
the first loopback argument. The slow capture validates byte ordering on
physical pins; it does not establish a full-speed electrical waveform.

The current board has no detected GPIO16-to-GPIO5 jumper, so physical byte
capture and merge remain pending. A successful timing benchmark is not a
substitute for that capture. Final validation, hosted review and merge are
also tracked in `agents/tasks/todo.md`.
