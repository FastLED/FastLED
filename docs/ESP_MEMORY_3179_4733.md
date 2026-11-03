# ESP32 driver binding and native SPI streaming

The typed `FastLED.add<Bus>()` API avoids registering unused drivers
([#4733](https://github.com/FastLED/FastLED/issues/4733)). Native clocked
SPI now sends large APA102/SK9822 frames through a 512-byte DMA staging buffer
([#3179](https://github.com/FastLED/FastLED/issues/3179)).

The measured candidate hardware run on 2026-11-02 passed both SPI chipsets. Matched
binding runs passed on ESP32-S3 and ESP32-C6. Final lint/checks and PR merge are
tracked separately in `agents/tasks/esp-memory-3179-4733.md`; these measurements
do not claim that a PR has merged. Complete RPC records, source hashes and ELF
hashes are in [the raw measurements](ESP_MEMORY_3179_4733.json).

## Typed driver binding

Use the typed overload when the application knows its driver at compile time:

```cpp
CRGB leds[32];

void setup() {
    fl::ChannelConfig cfg(
        fl::makeClockless<fl::TIMING_WS2812_800KHZ>(4), leds, GRB);
    auto channel = FastLED.add<fl::Bus::AUTO>(cfg);
    // FastLED retains channel; leds must remain alive while it is registered.
}
```

`Bus::AUTO` resolves to the platform's compile-time default. Explicit buses,
typed chipset configurations, and a `Which` instance argument are also
supported. An erased configuration with the wrong chipset family is rejected
before registering a driver. The existing `FastLED.add(cfg)` runtime overload
continues registering all available drivers so runtime selection stays usable.
The selected channel, manager and driver still allocate their own required
storage; this API does not promise allocation-free channels.

Both benchmark modes explicitly selected the same default bus, pin, 32 red
pixels, timing and twenty-frame workload. The returned actual driver name
confirmed RMT on S3 and PARLIO on C6. Driver counts started at zero.

| Board / actual backend | Runtime drivers | Typed drivers | Runtime registration | Typed registration | Registration saved | Steady SRAM saved |
|---|---:|---:|---:|---:|---:|---:|
| ESP32-S3 / RMT | 6 | 1 | 12,016 B | 2,840 B | 9,176 B | 9,180 B |
| ESP32-C6 / PARLIO | 5 | 1 | 11,540 B | 916 B | 10,624 B | 10,636 B |

Registration is the internal free-heap difference across the public `add`
call. Steady SRAM is the difference between modes after frame three; each run
remained flat from that point through frame twenty. Initial driver/SDK setup
is reported separately in the raw records.

These are heap measurements in the full AutoResearch fixture, whose other RPC
methods reference many drivers. Its deployed application image was 208 B
larger for typed S3 and 160 B larger for typed C6. Those images do not establish
a minimal-sketch flash saving. The C6 records have identical binding fixture
and `FastLED.h` hashes; unrelated native SPI source changed between the runs.
The measured C6 channel used PARLIO in both cases. All recorded input hashes
are retained rather than representing those binaries as byte-identical source
trees.

## Native clocked SPI

This changes the native `SPISingleESP32` / `SpiChannelEngineAdapter` path used
by explicit-host APA102/SK9822 `addLeds()` calls. `Bus::SPI` on ESP32 names the
separate WS2812-over-SPI driver. `Bus::FLEX_IO` can select LCD/I2S/PARLIO SPI;
neither is a substitute for the native path measured here.

The adapter now advances transmissions asynchronously through `poll()`.
Native SPI prequeues descriptors that reference a single DMA-capable staging
buffer. The SDK post-transaction ISR refills that buffer before starting the
next transaction. It does not call the task-only SPI queue API from an ISR.
The source channel remains owned until every queued result is drained,
including after timeouts or partial submission failures. Cleanup occurs in
task context, and final completion wakes the channel manager.

Staging is bounded at 512 B. Descriptor count and SDK queue storage scale with
`ceil(frame_bytes / 512)`, within the existing 65,536-byte native frame limit;
total driver storage is therefore not constant with frame length. The RGB
source and encoded channel frame remain allocated. Buses initialized by other
owners, and targets whose internal SRAM needs unverified ISR cache maintenance,
retain the established block path. Other platform backends retain their
fallback behavior.

The baseline restored four SPI production files from `dfbe01557` in the staged
library. It used the same public explicit-host `addLeds()` workload as the
candidate. The S3 independently received SPI2's MOSI/SCLK pads through SPI3
slave DMA, with input enabled alongside output. A software CS encloses the
whole frame, including all chunk boundaries. No physical thousand-LED strip
or extra jumper is needed for this pad-level byte-equivalence test. It does
not characterize electrical integrity along a long physical strip.

Each chipset sent twenty changing 1,000-pixel frames. Every frame contained
4,132 bytes / 33,056 bits, including start and end framing. Baseline and measured
candidate both captured every bit with zero byte mismatches and flat steady
heap.

| S3 native SPI measurement | Block baseline | Measured streaming | Change |
|---|---:|---:|---:|
| Scoped transfer DMA/internal allocation peak | 4,576 B | 1,044 B | 4.38× smaller |
| Free internal heap after prime | 118,256 B | 117,664 B | 592 B more retained |
| Free internal heap with receiver, steady | 112,892 B | 112,300 B | 592 B more retained |
| First-show scoped internal allocation peak | 12,124 B | 9,452 B | 2,672 B smaller |
| APA102 median `show()+wait()` | 10,119.5 µs | 9,374.5 µs | 7.36% shorter |
| SK9822 median `show()+wait()` | 10,089.5 µs | 9,364 µs | 7.19% shorter |
| Deployed AutoResearch application image | 1,918,064 B | 1,920,416 B | +2,352 B |

The 4.38× claim applies to transient transfer allocation, including measured
allocator/descriptor overhead, not to total SRAM. The extra 592 B of retained
state offsets part of the transient saving: estimated steady-transfer headroom
improved by 2,940 B from these two allocation deltas; this is not a directly
sampled whole-system peak. The source frame is common to both runs.

Scoped internal peaks use `MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT`; DMA peaks
use `MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA`. IDF local low-watermarks aggregate
per-region minima and are a conservative allocation measure, not a simultaneous
sample of whole-system peak RAM. `getFreeHeap().free_sram` uses
`MALLOC_CAP_INTERNAL`; only same-mask values are subtracted. Application image
bytes are the uncompressed bytes deployed at `0x10000`, not ELF file size,
attributed flash, or physical static RAM. No Blink footprint claim is made.

The host acceptance gate requires measured watermarks and
`peakDmaBytes * 3 <= frameBytes`, as well as the firmware's full-frame and
steady-heap checks. It rejects the recorded full-buffer baseline and accepts
the final streaming result. Timing is retained for matched comparison rather
than enforcing a board-independent latency threshold.

## Reproduction and provenance

The attached S3 was `/dev/ttyACM1`, USB serial `D8:3B:DA:41:64:C0`; C6 was
`/dev/ttyACM0`, serial `8C:BF:EA:CF:87:B4`. Re-enumerate and identify boards
before reusing these port names. The S3 profile used pioarduino release
`55.03.311`; C6 used `55.03.35`. Both used native USB Serial/JTAG and the
AutoResearch 16-KiB loop-stack setting. Generated profile hashes are recorded.

```bash
fbuild port scan
bash autoresearch esp32s3 --driver-binding typed --upload-port /dev/ttyACM1 --timeout 120s
bash autoresearch esp32s3 --driver-binding runtime --upload-port /dev/ttyACM1 --timeout 120s
bash autoresearch esp32c6 --driver-binding typed --upload-port /dev/ttyACM0 --timeout 120s
bash autoresearch esp32c6 --driver-binding runtime --upload-port /dev/ttyACM0 --timeout 120s
bash autoresearch esp32s3 --spi-streaming --upload-port /dev/ttyACM1 --timeout 120s
```

The SPI runner resets between chipsets, prints source/ELF SHA-256 provenance,
and returns nonzero on failed wire, heap or memory-budget checks. Ordinary
AutoResearch driver registration remains unchanged outside these isolated
benchmark modes. GPIO bring-up passed on S3 GPIO 1→2 and C6 GPIO 0→1; all four
S3 explicit-host/registration-order controls passed on both the block baseline
and final-source normal firmware. The existing `bash autoresearch esp32s3 --spi`
clockless-SPI regression also passed. The isolated memory firmware skips normal
GPIO/RX priming and is not used for these generic RX controls.

Measured SPI ELF SHA-256:
`7c38f75157d32d2e52da251df28a85b75249ba1744db26b4bc1c8187d4425b01`.
Matched ELFs/maps are retained locally under `.build/memory-evidence/` in
`spi-baseline`, `spi-final`, and the four `binding-*` directories. Earlier
stale candidate runs are excluded from the committed measurement record.

Native debug validation passed 329/329 unit binaries and 95/95 examples.
Affected Python validation passed 291 tests. The existing native SPI destructor
probe additionally passed all eight debug/release cases after its SDK shim was
updated for the new streaming dependencies; CI first exposed the missing shim
headers, and the failure was reproduced locally before correction. C++ lint passed with `bash lint --cpp --no-rust` after the Rust-backed
linter twice exceeded its 300-second deadline under concurrent host load.
The other full-lint lanes passed. See the task record for landing status.

The measurements were captured before review-only warning/help, argument
validation, exact-artifact lookup and header-layout corrections. Recorded hashes
identify the measured inputs; those later corrections do not change the SPI
chunking algorithm or typed driver selection.
