# fl::task API Design Document

## 1. Overview

This document outlines the proposed design changes to the `fl::task` API to support:

* **Default handlers** when `.then()` or `.catch_()` callbacks are not provided.
* **Source tracing** via optional file/line metadata, attached only when requested to save memory.
* A convenient **TracePoint** alias and **FL\_TRACE** macro to simplify source-location overloads.

## 2. Default Callback Behavior

### 2.1 Problem

Users may forget to set a `.then()` or `.catch_()` callback on a task. Without handlers, errors or missing callbacks silently pass.

### 2.2 Proposal

* If `.then()` is *never* called on a task, the scheduler will invoke a **default-then** handler that logs:

  > "\[fl::task] Warning: no `then()` callback set for Task#<ID> \[at file\:line if traced]"

* If `.catch_()` is *never* called, the scheduler will invoke a **default-catch** handler that logs:

  > "\[fl::task] Warning: no `catch_()` callback set for Task#<ID> \[at file\:line if traced]"

* When a task owns file/line metadata, the warnings will include `(file.cpp:123)`.

## 3. Trace Metadata & Memory Optimization

### 3.1 New Constructor

Add a constructor overload:

```cpp
// file/line optional
task(TaskType type, int interval_ms, const char* file, int line);
```

* **file** and **line** are stored in an optional `fl::unique_ptr<TaskTrace>`.
* `TaskTrace` holds `{ const char* file; int line; }`.
* Common no-trace path keeps the pointer null to minimize memory.

### 3.2 Naming & Logging

* If trace metadata is present, default handlers will print:

  > "\[fl::task] Warning: no `then()` callback for Task#<ID> launched at file.cpp:123"

* Otherwise they print without location.

## 4. TracePoint Alias & FL\_TRACE Macro

### 4.1 TracePoint Alias

To avoid repeating the full `fl::pair<const char*,int>` everywhere, introduce:

```cpp
namespace fl {
    /// (file, line) source trace
    struct TracePoint: public fl::pair<const char*, int> {
        TracePoint(const char* file, int line)
            : fl::pair<const char*, int>(file, line) {}
    };
}
```

### 4.2 FL\_TRACE

Define:

```cpp
#define FL_TRACE  fl::TracePoint(__FILE__, __LINE__)
```

This expands to the caller’s source location automatically.

### 4.3 Overloaded API

Extend static builders to accept `TracePoint`:

```cpp
static task every_ms(int interval_ms);
static task every_ms(int interval_ms, TracePoint trace);

static task at_framerate(int fps);
static task at_framerate(int fps, TracePoint trace);

static task before_frame();
static task before_frame(TracePoint trace);

static task after_frame();
static task after_frame(TracePoint trace);
```

* The overloads forward `trace.first`/`trace.second` into the trace-aware constructor.
* Callers write:

  ```cpp
  fl::task::every_ms(100, FL_TRACE)
  ```

  to capture filename and line automatically.

## 5. Class Diagram & Members

```cpp
namespace fl {

struct TaskTrace { const char* file; int line; };

class task {
public:
  // constructors
  task(TaskType t, int interval);
  task(TaskType t, int interval, const char* file, int line);

  // builders (overloads)
  static task every_ms(int ms);
  static task every_ms(int ms, TracePoint tr);
  static task at_framerate(int fps);
  static task at_framerate(int fps, TracePoint tr);
  static task before_frame();
  static task before_frame(TracePoint tr);
  static task after_frame();
  static task after_frame(TracePoint tr);

  // fluent API
  task& then(fl::function<void()> fn);
  task& catch_(fl::function<void(const Error&)> fn);
  task& cancel();

private:
  int                           mTaskId;
  bool                          mCanceled;
  // Update: DO NOT store the TaskTrace, you will store the
  // optional label if the Task is traced. The label will be generated from the
  // file-line information, with the path prefix striped out.
  // See FL_DBG().
  // Instead of a unique ptr to TaskTrace, use a unique ptr to a string.
  // This will allow us to store the label in the scheduler, and the file/line
  // information in the TaskTrace.
  fl::unique_ptr<TaskTrace>     mTrace;      // null if no trace
  bool                          mHasThen;
  bool                          mHasCatch;
};

} // namespace fl
```

## 6. Scheduler Integration

* Scheduler checks `mHasThen`/`mHasCatch` on task expiration or error.
* If false, invokes default handlers with optional trace:

  * `Scheduler::warn_no_then(mTaskId, mTrace)`
  * `Scheduler::warn_no_catch(mTaskId, mTrace)`

## 7. Example Usage in **app.cpp**

```cpp
// @file app.cpp
// @brief Simple button-reactive FastLED color selection with low brightness

#include <FastLED.h>
#include <fl/task.h>
#include "button_input.h"

#define NUM_LEDS 60
#define DATA_PIN 5
CRGB leds[NUM_LEDS];
CRGB current_color = CRGB::Black;

void setup() {
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(16);
    button_input::begin();

    // On button press — trace enabled
    auto colorPicker = fl::task::every_ms(1, FL_TRACE)
      .then([] {
          if (button_input::was_pressed()) {
              current_color = CRGB(random8(), random8(), random8());
          }
      });

    // Display at 60fps — trace enabled
    auto displayTask = fl::task::at_framerate(60, FL_TRACE)
      .then([] {
          fill_solid(leds, NUM_LEDS, current_color);
          FastLED.show();
      });
}

void loop() {
    fl::asyncrun();
}
```

* If `.catch_()` is omitted, at runtime you'll see:

  > `[fl::task] Warning: no catch_() callback set for Task#<ID> launched at app.cpp:<line>`

---



# Supplimental Design Document

TracePoint: a `pair<const char*,int>` -> fl::tuple<const char*, int, uint32_t>  // uin32_t is the clock millis time

This will allow better tracepoints. And to see long running tasks.


Here’s a minimal, header‑only C++11‑style `tuple` implementation (in namespace `fl`) along with `get<>`, `tuple_size`, `tuple_element`, and `make_tuple`. You can drop this into your codebase as `fl/tuple.h`:

```cpp
#pragma once

#include <cstddef>
#include <utility>
#include <type_traits>

namespace fl {

// Forward declaration
template<typename... Ts> struct tuple;

// Empty‑tuple specialization
template<>
struct tuple<> {};

// Recursive tuple: head + tail
template<typename Head, typename... Tail>
struct tuple<Head, Tail...> {
    Head head;
    tuple<Tail...> tail;

    tuple() = default;

    tuple(const Head& h, const Tail&... t)
      : head(h), tail(t...) {}

    tuple(Head&& h, Tail&&... t)
      : head(std::move(h)), tail(std::forward<Tail>(t)...) {}
};

// tuple_size
template<typename T>
struct tuple_size;

template<typename... Ts>
struct tuple_size< tuple<Ts...> > : std::integral_constant<std::size_t, sizeof...(Ts)> {};

// tuple_element
template<std::size_t I, typename Tuple>
struct tuple_element;

template<typename Head, typename... Tail>
struct tuple_element<0, tuple<Head, Tail...>> {
    using type = Head;
};

template<std::size_t I, typename Head, typename... Tail>
struct tuple_element<I, tuple<Head, Tail...>>
  : tuple_element<I-1, tuple<Tail...>> {};

// get<I>(tuple)
template<std::size_t I, typename Head, typename... Tail>
typename std::enable_if<I == 0, Head&>::type
get(tuple<Head, Tail...>& t) {
    return t.head;
}

template<std::size_t I, typename Head, typename... Tail>
typename std::enable_if<I != 0, typename tuple_element<I, tuple<Head, Tail...>>::type&>::type
get(tuple<Head, Tail...>& t) {
    return get<I-1>(t.tail);
}

// const overloads
template<std::size_t I, typename Head, typename... Tail>
typename std::enable_if<I == 0, const Head&>::type
get(const tuple<Head, Tail...>& t) {
    return t.head;
}

template<std::size_t I, typename Head, typename... Tail>
typename std::enable_if<I != 0, const typename tuple_element<I, tuple<Head, Tail...>>::type&>::type
get(const tuple<Head, Tail...>& t) {
    return get<I-1>(t.tail);
}

// rvalue overloads
template<std::size_t I, typename Head, typename... Tail>
typename std::enable_if<I == 0, Head&&>::type
get(tuple<Head, Tail...>&& t) {
    return std::move(t.head);
}

template<std::size_t I, typename Head, typename... Tail>
typename std::enable_if<I != 0, typename tuple_element<I, tuple<Head, Tail...>>::type&&>::type
get(tuple<Head, Tail...>&& t) {
    return get<I-1>(std::move(t.tail));
}

// make_tuple
template<typename... Ts>
tuple<typename std::decay<Ts>::type...>
make_tuple(Ts&&... args) {
    return tuple<typename std::decay<Ts>::type...>(std::forward<Ts>(args)...);
}

} // namespace fl
```

**Usage example:**

```cpp
#include "fl/tuple.h"
#include <iostream>

int main() {
    // create a tuple of (int, const char*, float)
    auto t = fl::make_tuple(42, "hello", 3.14f);

    // access elements
    int    i = fl::get<0>(t);
    const char* s = fl::get<1>(t);
    float  f = fl::get<2>(t);

    std::cout << i << ", " << s << ", " << f << "\n";
}
```

This `tuple` supports:

* Arbitrary type packing via variadic templates
* Compile‑time size and element access (`tuple_size`, `tuple_element`)
* `get<I>()` for mutable, const, and rvalue tuples
* `make_tuple()` to deduce and decay types for you

You can extend it further (e.g. `tie()`, `apply()`, structured bindings) as needed.
