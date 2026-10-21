# ESP32 RMT4 driver

The IDF 4.x backend implements `IChannelDriver` for clockless LED output.
`ChannelEngineRMT4Impl` queues source `ChannelData`, starts asynchronous SDK
transfers and polls their completion. IDF 5.x uses the separate RMT5 backend.

## Legacy API footprint

`FastLED.addLeds<>()` uses the existing slim bridge in direct mode by default.
All legacy strips enqueue before `endShowLeds()` flushes their shared driver
and waits for completion. Strips still transmit in parallel when hardware
capacity allows; polling also advances strips waiting for a hardware channel.
Standalone `controller.showLeds()` uses the same completion hook.

A legacy-only sketch therefore does not retain `ChannelManager` just to render
its LEDs. `FastLED.add()` continues to use the Channel API and its manager.
Mixed sketches retain that manager as required by their Channel API calls.

Direct legacy controllers use their controller's `setEnabled()` setting. They
do not participate in the ChannelManager registry's runtime driver enable,
disable, exclusive selection or enumeration. Sketches needing those controls
for legacy controllers can build with `FL_SLIM_BRIDGE_DIRECT_SHOW=0` to restore
the managed bridge. This choice does not change RMT4's translator, allocation,
reset timing or transmission timeout handling.

Matched 96-RGB-LED Lolin D32 reporter (espressif32 6.13.0, Arduino 2.0.17,
IDF 4.4.7), compared with PR #4877 at `b8ae6a398`:

| Build | Flash bytes | Static RAM bytes |
|---|---:|---:|
| Managed legacy bridge before this change | 295,670 | 23,844 |
| Direct legacy bridge with completion polling | 289,426 | 23,636 |
| Saving | 6,244 | 208 |

`bash bloat` confirms no executable `ChannelManager` symbols in the direct
reporter. This is static memory accounting; the SDK translator and frame
storage still allocate heap. The managed override on the new code measures
295,686 flash / 23,844 static RAM bytes.

## Interrupt ownership

TX and RX both use the SDK's RMT driver and its single shared ISR. TX installs
an IRAM byte translator with `rmt_translator_init` and starts a transfer with
`rmt_write_sample(..., false)`. It does not install a competing custom ISR.

The translator converts bytes into MSB-first RMT symbols using the original
chipset timing conversion. Per-channel timing symbols and refill timestamps
live in internal DRAM. Explicit out-of-line IRAM callbacks avoid the Xtensa
literal-pool placement failures of inline class methods. No virtual calls or
SDK context lookup occur in the translator. The legacy late-refill check
ends a frame if more than 1.5 times its expected half-buffer duration elapses.

The SDK owns the refill buffer, threshold interrupts and stop symbol. This
requires a bounded heap buffer of `blocks * words_per_block * 4` bytes per
configured TX channel, plus SDK bookkeeping; it does not pre-encode the whole
LED strip. A classic ESP32 channel using two blocks needs 512 bytes for that
buffer. Internal-DRAM source data is passed directly to the SDK. A PSRAM source
requires an additional internal-DRAM copy of the raw bytes for cache-safe ISR
access; allocation failure returns `ERROR`. The channel retains its source
`ChannelData` and any internal copy until transmission has completed or the SDK
driver has been stopped and uninstalled.

## Memory ownership and channel reuse

`RmtMemoryManager4` tracks both aggregate symbol-word allocations and actual
contiguous hardware-block reservations. TX and RX must reserve their physical
span before hardware configuration and release it on failure or destruction.
A two-block channel at hardware channel zero therefore excludes channel one.
The manager takes the SoC's block size and channel geometry from the SDK.

TX normally receives two blocks. The existing network tracker remains a stub;
if it eventually requests three blocks, the SDK derives its refill halves
from the actual configured size. Concurrent capacity depends on these block
reservations and any RX allocations; extra strips wait for a TX channel.

Idle TX channels retain their SDK driver and memory reservation for reuse.
Every reuse reconnects the GPIO, even for the same pin. GPIO inputs remain
enabled alongside TX output so an RMT RX channel can observe a same-pin
loopback. Configuring another pin disconnects the previous output first.

## Completion and failure

### Legacy asynchronous output

On the RMT4 backend, `FL_RMT4_ASYNC` defaults to `1`. Legacy
`FastLED.addLeds<>()` controllers submit their batch from `FastLED.show()`
without waiting for its transmission or reset interval to finish. A single
lazy FreeRTOS worker services the existing SDK driver, advances strips that
exceed physical TX capacity, and releases completed frames. Application calls
to `delay()`, `yield()` or another `show()` are not required for progress.

Pixel values are encoded before submission. Editing the caller's pixel array
after `show()` returns does not change the submitted frame. Submitting the
next frame can wait for a controller's previous encoded buffer to become
available; this provides backpressure instead of accumulating an unbounded
queue of frames. Retained encoded data outlives controller removal.

Use `FastLED.wait()` to wait indefinitely for submitted output, or
`FastLED.wait(timeoutMs)` for a bounded wait. A zero timeout means no deadline.
The bounded overload returns false on a timeout or a reported driver failure. A
timeout does not cancel an active transfer. Backend `poll()` reports an error
once; the completion wait retains failure status until an idle driver accepts
a new valid batch. Successful completion includes the chipset reset interval.

Define `FL_RMT4_ASYNC=0` consistently for the sketch and library to select
blocking output without the background worker. This setting applies to RMT4;
it does not change other platforms' completion policies. The Channel API uses
the same selected RMT4 engine and retains its manager-based dispatch. Legacy
output registers only a completion-wait callback, so waiting does not itself
require constructing the ChannelManager.

The worker uses a 2,048-byte task stack, allocated from runtime heap on first
submission, plus FreeRTOS task and semaphore bookkeeping. These allocations
are separate from static RAM and per-channel SDK refill storage. They remain
available for later frames until the driver is destroyed.

`poll()` uses `rmt_wait_tx_done(channel, 0)` without blocking. After completion,
the output stays connected at the configured idle-low level for at least the
chipset's `reset_us` before the channel is released for reuse. The legacy
`WAIT_TIME` setting can extend that interval through `SlimBridgeController`.

`FASTLED_RMT4_TRANSMISSION_TIMEOUT_MS` defaults to 2000 milliseconds; zero
disables the timeout. A timeout stops and uninstalls the SDK driver before
releasing the source buffer. This also resets the SDK semaphore, preventing a
later transfer from blocking on the timed-out transfer. Setup failures and a
fully unavailable physical pool return `ERROR` instead of an endless pending
queue.

The refill deadline uses the shared, ISR-safe ESP timer. CPU cycle counters
cannot be compared across the task and interrupt cores. The SDK performs its
initial translation before starting hardware; after its nonblocking start
returns, the first deadline is rebased without overwriting an interrupt that
has already advanced it. This excludes software preload overhead from the
hardware refill interval. The first and subsequent deadline checks remain
enabled. A late refill ends the affected frame and reports an error, rather
than presenting truncated output as successful completion.

IRAM interrupt flags request cache-safe SDK handling, but behavior also depends
on the SDK build configuration and on the first RMT channel installed. Device
loopback checks do not establish reliability under Wi-Fi or flash-cache stress;
those require separate measurements. The existing IDF 3-only
`FASTLED_ESP32_FLASH_LOCK=1` setting holds the flash-operation lock until all
active transmissions finish. IDF 4 has no implementation of that setting.

## Measured async footprint

Matched Lolin D32 builds of the 96-RGB-LED reporter, using fbuild,
espressif32 6.13.0 / Arduino 2.0.17 / IDF 4.4.7, with diagnostics disabled:

| Consumer | Flash bytes | Physical static RAM bytes |
|---|---:|---:|
| Slim legacy, `FL_RMT4_ASYNC=0` | 290,306 | 23,708 |
| Slim legacy, default async | 293,006 | 23,716 |
| Channel API, default async | 363,022 | 25,236 |

Async adds 2,700 flash bytes and 8 static RAM bytes versus matched blocking
output. Both legacy ELF files contain zero executable `ChannelManager`
functions. The Channel consumer also registers other available backends, so
its entire difference cannot be attributed to ChannelManager alone. Runtime
heap, the 2 KB worker stack and per-frame encoded storage are additional costs.

Matched device harness heap checks before/after one 2,000-LED legacy-only
submission show 110,208 / 109,424 free bytes for blocking and 109,584 / 106,384
for async. Thus async retains 3,040 additional heap bytes after completion,
including 2,048 stack bytes; 624 bytes of that difference exist before first
submission. Minimum free heap was 57,540 / 54,556 bytes, and the largest free
block remained 65,524 bytes in both modes. These are diagnostic AutoResearch
builds on the same classic ESP32 and IDF 4.4 toolchain, rather than heap totals
for the small size-reporter sketch. Frame/source allocations in the probe are
released before the final heap check; worker and SDK resources remain cached.

Encoding reserves the complete frame before writing. Five 2,000-RGB-LED strips
retain exactly 30,000 encoded bytes; geometric vector growth previously retained
40,460 bytes for that workload. A failed reservation rejects the frame and
reports a batch failure instead of transmitting a partially encoded frame.

Classic ESP32 device validation includes short same-pin RX byte comparisons,
2,000-LED completion, and five strips sharing three TX slots while caller data
and controllers are destroyed. The latter translates all 30,000 bytes and
releases all retained frames without application polling. Diagnostic counters
observe five completed reset holds with no violations. Mixed APIs, standalone
show, enable/disable, replacement and backpressure pass. Task/mutex allocation,
SDK installation failure and a real transfer timeout release their resources;
the timeout test transmits 120 of 12,000 bytes before teardown and recovers on
the same driver. The 2 KB stack retains at least 1,196 bytes unused in the
normal scenario set and 1,236 bytes in the timeout test. These are measured
workloads, not a guarantee for every SDK configuration or interrupt load.

## RX validation limits

The legacy RX adapter captures one hardware block per frame (64 symbols on
classic ESP32). Its bounded SDK ring buffer includes item-header headroom;
larger software requests do not increase physical capture capacity. All SDK
items, including empty idle packets, are returned to the ring buffer. A completed
capture stops RX; rearming discards stale packets. The decoder joins HIGH/LOW
durations across SDK symbol boundaries and rejects physical overflow before
allowing an SDK-omitted final LOW. Injected edge streams are independent of that
physical capacity check.

Short same-pin loopback validates byte correctness. Longer-strip refill smoke
validates forward progress, without establishing full waveform or stress parity.

## Files

| File | Purpose |
|---|---|
| `channel_driver_rmt4.h` | Engine interface and per-channel task state |
| `channel_driver_rmt4.cpp.hpp` | Byte translators, queueing and polling |
| `irmt4_peripheral.h` | Peripheral lifecycle interface |
| `rmt4_peripheral_esp.cpp.hpp` | ESP-IDF lifecycle implementation |
| `rmt_memory_manager_4.h` | Word ledger and physical-span reservations |
| `idf4_clockless_rmt_esp32.h` | Legacy `FastLED.addLeds` bridge |

Reference: [Espressif IDF 4.4 RMT documentation](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32/api-reference/peripherals/rmt.html).
