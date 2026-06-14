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
#include "fl/log/log.h"

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

    // Emit a literal-only warning so the autoresearch harness can verify
    // the FL_WARN log pipeline reaches the host. `FL_WARN_LIT` bypasses
    // the full sstream/log_emit chain (which is no-op'd on Low-memory
    // targets) and goes straight to `fl::println(const char*)`, so it
    // works on the LPC845 flash budget. See FastLED #3002.
    FL_WARN_LIT("FL_WARN: LPC845 bring-up OK");

    static fl::Remote remote(
        fl::createSerialRequestSource(),
        fl::createSerialResponseSink("REMOTE: "));
    g_remote = &remote;
    remote.bind("echo", [](int v) -> int {
        // Re-emit the FL_WARN marker on every echo so the autoresearch
        // harness can observe it even after the boot-banner drain step
        // (see ci/autoresearch/phases.py — `_run_bring_up_tests` resets
        // the input buffer twice before sending the first request).
        FL_WARN_LIT("FL_WARN: echo invoked");
        return v;
    });

    // probePair(packed) -> 1 if pins bridged, 0 otherwise.
    //   packed = (out_pin << 8) | in_pin    (single int to reuse the
    //   existing JsonToType<int> instantiation that `echo` already
    //   pulled in — adding a 2-int binding would re-instantiate the
    //   tuple/arg-converter chain and blow 1.6 KB of flash budget.)
    // Driving LOW with a pull-up on the input is the noise-tolerant
    // probe direction — no contention risk and floating inputs default
    // to HIGH. Used by the host-side pin-bridge scan during bring-up.
    remote.bind("probePair", [](int packed) -> int {
        int out_pin = (packed >> 8) & 0xFF;
        int in_pin  = packed & 0xFF;
        // Skip UART pins so the probe can't accidentally tank serial.
        if (out_pin == PIN_SERIAL_TX || out_pin == PIN_SERIAL_RX ||
            in_pin  == PIN_SERIAL_TX || in_pin  == PIN_SERIAL_RX) {
            return 0;
        }
        if (out_pin == in_pin) return 0;

        // Pre-charge: drive BOTH pins HIGH for 200 µs to flush residual
        // capacitance left over from the previous probe pair. Without
        // this, scanning gives "succeeds both ways" false positives for
        // adjacent pins on the breakout header because each probe leaves
        // a small charge that biases the next test.
        pinMode(out_pin, OUTPUT);
        pinMode(in_pin, OUTPUT);
        digitalWrite(out_pin, HIGH);
        digitalWrite(in_pin, HIGH);
        delayMicroseconds(200);

        // Probe: drive out LOW, switch in to INPUT_PULLUP, give the pull-up
        // 2 ms to win against breadboard/PCB stray capacitance. A real
        // jumper wire pulls in LOW within nanoseconds; parasitic coupling
        // doesn't survive a 2 ms pull-up fight.
        digitalWrite(out_pin, LOW);
        pinMode(in_pin, INPUT_PULLUP);
        delay(2);
        int connected = (digitalRead(in_pin) == LOW) ? 1 : 0;

        // Cleanup: both back to plain INPUT (high-Z, no pull).
        pinMode(out_pin, INPUT);
        pinMode(in_pin, INPUT);
        return connected;
    });
}

void loop() {
    fl::Watchdog::instance().feed();
    if (g_remote) {
        g_remote->update(millis());
    }
}
