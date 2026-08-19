# FastLED Async Tasks (`fl::task`)

A cooperative scheduler for sketches that need to do more than one thing at a time.

Instead of a `loop()` full of `delay()` calls and interleaved `millis()` comparisons, you
register small callbacks that each own their own schedule. `fl::task::run()` executes
whichever ones are due and returns. Adding a feature means adding a task — the existing
tasks don't change.

Everything here is in namespace `fl::task` and is pulled in by `#include <FastLED.h>`.

## Quick Start

```cpp
#include <FastLED.h>
#include "fl/task/task.h"

#define NUM_LEDS 60
CRGB leds[NUM_LEDS];

void setup() {
    FastLED.addLeds<WS2812, 6>(leds, NUM_LEDS);

    // Two blinkers on independent schedules
    fl::task::every_ms(700).then([] {
        static bool on = false;
        on = !on;
        leds[0] = on ? CRGB::Red : CRGB::Black;
    });

    fl::task::every_ms(400).then([] {
        static bool on = false;
        on = !on;
        leds[1] = on ? CRGB::Blue : CRGB::Black;
    });

    // One task owns the hardware push
    fl::task::at_framerate(60).then([] { FastLED.show(); });
}

void loop() {
    fl::task::run();  // run whatever is due, then return
}
```

A runnable version of this is in [`examples/Async`](../../../examples/Async).

## Why

The usual embedded tutorial teaches a single polling loop. That works until the second
feature arrives. Once WiFi, MQTT, a button, and an animation all share one `loop()`, the
timing of each is entangled with the others: a blocking read stalls the animation, and
every new feature has to be woven through the code already there.

With tasks, a new feature is a new registration:

```cpp
fl::task::every_ms(50).then([] { mqtt.poll(); });
```

Nothing above it changes.

## `fl::task::run()` — the pump

**`run()` must be called from `loop()` for time-based tasks.** Frame tasks are the exception:
`before_frame()` and `after_frame()` are pumped automatically by the FastLED frame lifecycle.
If you never call `run()`, `every_ms()` and `at_framerate()` tasks never fire.

```cpp
void loop() {
    fl::task::run();
}
```

| Call | Effect |
|------|--------|
| `run()` | Pump everything, then yield for 1 ms (the default) |
| `run(0)` | Pump everything, no yield |
| `run(250, ExecFlags::SYSTEM)` | OS-level yield only — for driver/DMA wait loops |
| `run(1000, ExecFlags::TASKS \| ExecFlags::COROUTINES)` | Pump tasks and coroutines, skip the OS yield |

`ExecFlags` selects which subsystems get pumped:

- `TASKS` — the scheduler (your `every_ms` / `at_framerate` callbacks) plus the executor
  (HTTP fetches, HTTP server, and other registered runners)
- `COROUTINES` — platform coroutines
- `SYSTEM` — a pure OS yield, so lower-priority work such as the WiFi/lwIP stack gets CPU
- `ALL` — all of the above (the default)

The 1 ms default yield is what keeps a busy render loop from starving the network stack.

> Some subsystems bump the scheduler on their own — the FX engine does so when an audio
> processor is attached, so audio samples are current for the frame being drawn. Treat that
> as an implementation detail: call `run()` yourself.

## Creating tasks

| Builder | Fires |
|---------|-------|
| `fl::task::every_ms(int interval_ms)` | Every `interval_ms`, forever |
| `fl::task::at_framerate(int fps)` | `fps` times a second, forever |
| `fl::task::before_frame()` | Before every FastLED render, until canceled |
| `fl::task::after_frame()` | After every FastLED render, until canceled |
| `fl::task::coroutine(const CoroutineConfig&)` | Once, on its own OS task (see below) |

Each builder has an overload taking a `TracePoint` — pass the `FL_TRACE` macro and the
scheduler can name the task in warnings, which is worth doing once you have more than a
few:

```cpp
fl::task::every_ms(100, FL_TRACE).then([] { /* ... */ });
```

## The fluent API

```cpp
auto handle = fl::task::every_ms(250, FL_TRACE)
    .then([] { /* the work */ })
    .catch_([](const fl::task::Error& e) { FL_WARN("task failed: " << e.message); });
```

- **`.then(callback)`** — sets the work *and registers the task with the scheduler*. There
  is no separate "add" step, and a task without a `.then()` never runs.
- **`.catch_(callback)`** — error handler. Optional, and it does not register anything on
  its own; always call `.then()`.
- **`.cancel()`** — marks the task dead. The scheduler drops it on the next `update()`.

The returned `Handle` is a shared handle — copying it is cheap, and letting it go out of
scope does *not* cancel the task, because the scheduler holds its own copy. Keep the handle
only if you intend to `.cancel()` or re-time it later:

```cpp
auto blink = fl::task::every_ms(500).then([] { toggle(); });
blink.set_interval_ms(100);   // speed it up
blink.cancel();               // stop it
```

`every_ms`, `at_framerate`, `before_frame`, and `after_frame` tasks recur until canceled.
Coroutines run once. Frame tasks do not require `run()`; engine frame events dispatch them
automatically.

Callbacks run on whichever thread called `run()` or initiated the frame and are not
preempted — keep each one short. A callback that blocks for 200 ms delays every other
task by 200 ms.

## Promises

Anything asynchronous returns a `fl::task::Promise<T>` with the same `.then()` / `.catch_()`
shape:

```cpp
#include "fl/net/http.h"

fl::net::http::fetch_get("http://fastled.io")
    .then([](const fl::net::http::Response& resp) {
        FL_WARN("got " << resp.text());
    })
    .catch_([](const fl::task::Error& err) {
        FL_WARN("fetch failed: " << err.message);
    });
```

The promise completes while the executor is pumped, so this still requires `run()` in
`loop()`. You can also create and settle your own:

```cpp
auto p = fl::task::Promise<int>::create();
// ... later, from wherever the answer arrives:
p.complete_with_value(42);
```

`Promise<T>::resolve(value)` and `Promise<T>::reject(error)` build already-settled promises,
which is handy for early returns and for tests.

## `await` — sequential async

Callbacks are awkward when the logic is genuinely sequential: *fetch, wait for the reply,
then apply it*. Both `await` forms give you that back, and both return a
`PromiseResult<T>` — check `ok()` before `value()`:

```cpp
auto result = fl::task::await_top_level(fl::net::http::fetch_get("http://fastled.io"));
if (result.ok()) {
    apply(result.value().text());
} else {
    FL_WARN("fetch failed: " << result.error().message);
}
```

| | Where | Cost |
|---|---|---|
| `await_top_level(p)` | Arduino `loop()` / `main()`, every platform | Busy-waits, calling `run(1000)` in a loop — other tasks still run, but the CPU is spinning |
| `await(p)` | Inside a coroutine — ESP32 and host builds | Blocks that coroutine only, at zero CPU cost; everything else keeps running |

Prefer `await()` when you have a coroutine to put the work on. `await_top_level()` refuses
to nest more than a few levels deep and gives up rather than hanging forever.

## Coroutines

A coroutine is a real OS task (a FreeRTOS task on ESP32, a `std::thread` on host builds),
so it can block on `await()` without stalling the animation.

```cpp
#include "fl/net/http.h"

auto worker = fl::task::coroutine({
    .func = [] {
        auto r = fl::task::await(fl::net::http::fetch_get("http://my-server/color"));
        if (r.ok()) {
            apply_color(r.value().text());
        }
    },
    .name = "color-fetch",
});
```

`CoroutineConfig` fields:

| Field | Default | Notes |
|-------|---------|-------|
| `func` | — | The body. Required. |
| `name` | `"task"` | Shows up in RTOS task listings |
| `stack_size` | `4096` | Bytes |
| `priority` | `5` | RTOS priority |
| `core_id` | unset | Pin to a CPU core on multi-core parts |
| `trace` | unset | `FL_TRACE` for diagnostics |

The handle is RAII: `worker.stop()` ends it, and so does letting the handle go out of
scope. From inside the body, `fl::task::exit_current()` self-deletes.

## Platform support

| | Scheduler (`every_ms`, `at_framerate`) | `await_top_level` | `coroutine()` + `await` |
|---|---|---|---|
| AVR / small MCUs | ✅ | ✅ | ❌ no-op |
| ESP32 family | ✅ | ✅ | ✅ FreeRTOS task |
| Teensy 4.x | ✅ | ✅ | ✅ |
| Other Arduino / ARM | ✅ | ✅ | ❌ no-op |
| Host / stub (tests) | ✅ | ✅ | ✅ `std::thread` |
| WASM | ✅ | ✅ | ✅ |

Code using coroutines still compiles everywhere; where there is no support,
`fl::task::coroutine()` is a no-op, so don't depend on the body having run.

## Gotchas

- **No `run()`, no time-based tasks.** Frame tasks follow FastLED frame events instead.
- **`.then()` is what registers.** A handle with only `.catch_()` is inert.
- **Don't block in a callback.** Everything else waits behind it. Long or blocking work
  belongs in a coroutine.
- **Intervals are floors, not guarantees.** A task due every 10 ms fires on the first
  `run()` at or after the deadline; a slow callback elsewhere pushes it out.
- **Timing comes from `millis()`.** Sub-millisecond scheduling is not what this is for.

## Files

| File | Contents |
|------|----------|
| `task.h` | `Handle`, the builders, `CoroutineConfig` |
| `scheduler.h` | `Scheduler` — task storage and dispatch |
| `executor.h` | `run()`, `ExecFlags`, `Runner`, `await`, `await_top_level` |
| `promise.h` | `Promise<T>` |
| `promise_result.h` | `PromiseResult<T>`, `Error` |

## See also

- [`examples/Async`](../../../examples/Async) — the quick start, runnable
- [`src/fl/net/http.h`](../net/http.h) — `fetch_get` and the HTTP server
- [`src/fl/audio/README.md`](../audio/README.md) — audio callbacks, which ride on this scheduler
