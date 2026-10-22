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

IRAM interrupt flags request cache-safe SDK handling, but behavior also depends
on the SDK build configuration and on the first RMT channel installed. Device
loopback checks do not establish reliability under Wi-Fi or flash-cache stress;
those require separate measurements. The existing IDF 3-only
`FASTLED_ESP32_FLASH_LOCK=1` setting holds the flash-operation lock until all
active transmissions finish. IDF 4 has no implementation of that setting.

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
