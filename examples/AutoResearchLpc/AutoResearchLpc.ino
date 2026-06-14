// FL_AGENT_ALLOW_NEW_EXAMPLE
//
// @filter
// require:
//   - board: lpc845brk
// @end-filter
//
// Gate FL_DBG to no-op. Without `RELEASE`, fl/log/log.h auto-sets
// FASTLED_FORCE_DBG=1, which expands every FL_DBG call site (inside
// fl::Remote etc.) through the fl::println formatting machinery and
// blows ~4 KB of flash budget. fbuild's nxplpc orchestrator does not
// yet propagate platformio.ini's `build_flags`, so it's defined here.
#define RELEASE 1
//
// examples/AutoResearchLpc/AutoResearchLpc.ino
//
// AutoResearch bring-up harness for NXP LPC845-BRK (Cortex-M0+, 64 KB Flash,
// 16 KB SRAM). Mirrors the JSON-RPC contract of the main AutoResearch.ino but
// strips out the LED-protocol matrix (RMT/PARLIO/SPI/etc.) that the LPC8xx
// family doesn't have peripherals for.
//
// Purpose: validate that on a freshly-brought-up board the basic transport
// (Serial.println, FastLED log pipeline, JSON-RPC echo) all work end-to-end
// through `bash autoresearch lpc845brk --bring-up`.
//
// RPC contract (over Serial @ 115200, JSON-RPC 2.0 framed by `\n`,
// responses prefixed with `REMOTE: `):
//
//   echo: [int] -> int
//     Returns the input integer unchanged. The bring-up harness sends
//     {"jsonrpc":"2.0","method":"echo","params":[N],"id":I} and verifies
//     the response is {"id":I,"result":N,"jsonrpc":"2.0"}.
//
// FL_DBG output from inside fl::Remote (e.g. "Stored request ID for echo")
// also reaches the host through this transport, demonstrating that the
// FastLED log pipeline is wired correctly. FL_WARN proper is gated behind
// SKETCH_HAS_LARGE_MEMORY which is off on LPC845 (flash budget); enabling
// it requires the fl::json fixed-point refactor tracked in FastLED #3002.

#include <Arduino.h>
#include "fl/remote/remote.h"
#include "fl/remote/transport/serial.h"
#include "fl/wdt/watchdog.h"

namespace {
fl::Remote* g_remote = nullptr;
}

void setup() {
    Serial.begin(115200);

    // Arm the LPC845 WWDT so a hung sketch unbricks itself on crash.
    // ~3 second window — long enough for any legitimate setup() work,
    // short enough that "device is dead" reboots within human attention
    // span during bring-up debugging. See FastLED #3002 follow-up.
    fl::Watchdog::instance().begin(3000);

    static fl::Remote remote(
        fl::createSerialRequestSource(),
        fl::createSerialResponseSink("REMOTE: "));
    g_remote = &remote;
    remote.bind("echo", [](int v) -> int { return v; });
}

void loop() {
    fl::Watchdog::instance().feed();
    if (g_remote) {
        g_remote->update(millis());
    }
}
