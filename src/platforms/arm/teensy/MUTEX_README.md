# Teensy Mutex Implementation

## Overview

The Teensy platform now supports optional **real mutex** implementations when threading libraries are available. This provides thread-safe synchronization for Teensy 3.x and 4.x boards.

## Supported Threading Libraries

### 1. FreeRTOS (Recommended - Preemptive)

**Platforms:** Primarily Teensy 4.x (Cortex-M7)
**Type:** Preemptive multitasking
**Thread Safety:** True concurrent thread safety

FreeRTOS provides preemptive multitasking where the scheduler can interrupt a running thread at any time to switch to another thread. This offers the strongest thread safety guarantees.

**Installation:**
```bash
# Arduino Library Manager or PlatformIO
# Search for: "FreeRTOS" or "Arduino_FreeRTOS_ARM"
```

**Usage:**
```cpp
#include <Arduino_FreeRTOS_ARM.h>  // Can be included in any order
#include <FastLED.h>

// Mutexes will automatically use FreeRTOS implementation
```

### 2. TeensyThreads (Cooperative)

**Platforms:** Teensy 3.x, 4.x
**Type:** Cooperative multitasking (yield-based)
**Thread Safety:** Cooperative synchronization

TeensyThreads uses cooperative multitasking where threads explicitly yield control. Thread switches only occur at yield points, not during arbitrary code execution.

**Important:** This is NOT preemptive multitasking. Interrupts can still occur at any time.

**Installation:**
```bash
# Arduino Library Manager or PlatformIO
# Search for: "TeensyThreads"
# GitHub: https://github.com/ftrias/TeensyThreads
```

**Usage:**
```cpp
#include <TeensyThreads.h>  // Can be included in any order
#include <FastLED.h>

// Mutexes will automatically use TeensyThreads implementation
```

### 3. Fallback (No Threading)

If neither FreeRTOS nor TeensyThreads is detected, FastLED automatically falls back to **fake mutexes** that provide basic lock tracking for debugging but are **NOT thread-safe**.

## Priority and Detection

The implementation automatically detects threading libraries in this priority order:

1. **FreeRTOS** (if `INC_FREERTOS_H` is defined) - highest priority
2. **TeensyThreads** (if `TEENSY_THREADS_H` is defined)
3. **Fake mutex** (fallback if neither is available)

**Key Point:** If both libraries are included, FreeRTOS takes priority because it provides preemptive multitasking.

## API

Both real and fake implementations provide the same API:

```cpp
namespace fl {

class MutexTeensy {
public:
    void lock();         // Block until lock acquired
    void unlock();       // Release the lock
    bool try_lock();     // Try to acquire without blocking (returns true/false)
};

class RecursiveMutexTeensy {
public:
    void lock();         // Block until lock acquired (same thread can lock multiple times)
    void unlock();       // Release one lock level
    bool try_lock();     // Try to acquire without blocking
};

}
```

## Usage Examples

### Basic Mutex

```cpp
#include <TeensyThreads.h>
#include <FastLED.h>

fl::MutexTeensy gLedMutex;
CRGB leds[NUM_LEDS];

void thread1() {
    while (true) {
        gLedMutex.lock();
        leds[0] = CRGB::Red;
        FastLED.show();
        gLedMutex.unlock();
        threads.yield();  // Cooperative: must explicitly yield
    }
}

void thread2() {
    while (true) {
        gLedMutex.lock();
        leds[0] = CRGB::Blue;
        FastLED.show();
        gLedMutex.unlock();
        threads.yield();
    }
}

void setup() {
    FastLED.addLeds<WS2812, DATA_PIN>(leds, NUM_LEDS);
    threads.addThread(thread1);
    threads.addThread(thread2);
}
```

### Recursive Mutex

```cpp
#include <Arduino_FreeRTOS_ARM.h>
#include <FastLED.h>

fl::RecursiveMutexTeensy gRecursiveMutex;

void recursiveFunction(int depth) {
    gRecursiveMutex.lock();

    if (depth > 0) {
        recursiveFunction(depth - 1);  // Can lock multiple times
    }

    gRecursiveMutex.unlock();
}

void task1(void* params) {
    while (true) {
        recursiveFunction(5);  // Locks 5 times recursively
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void setup() {
    xTaskCreate(task1, "Task1", 1000, NULL, 1, NULL);
    vTaskStartScheduler();
}
```

### RAII Lock Guard (Recommended)

```cpp
#include <TeensyThreads.h>
#include <FastLED.h>

fl::MutexTeensy gMutex;

void criticalSection() {
    fl::unique_lock<fl::MutexTeensy> lock(gMutex);

    // Mutex automatically locked here
    leds[0] = CRGB::Green;
    FastLED.show();

    // Mutex automatically unlocked when 'lock' goes out of scope
}
```

## Implementation Details

### FreeRTOS Implementation

- Uses `xSemaphoreCreateMutex()` for standard mutexes
- Uses `xSemaphoreCreateRecursiveMutex()` for recursive mutexes
- Implements priority inheritance to prevent priority inversion
- Thread-safe with preemptive scheduling

**Files:**
- `mutex_teensy.h` - Class declarations
- `mutex_teensy.cpp` - FreeRTOS implementation (only compiled when FreeRTOS detected)

### TeensyThreads Implementation

- Uses `Threads::Mutex` for standard mutexes
- Implements recursive mutex manually (TeensyThreads doesn't provide one natively)
- Tracks thread ID and lock count for recursion
- Yields to other threads when blocked (cooperative)

**Files:**
- `mutex_teensy.h` - Complete inline implementation (no .cpp needed)

### Fake Implementation

- Uses `bool mLocked` for standard mutex tracking
- Uses `int mLockCount` for recursive mutex tracking
- Provides assertions to catch locking bugs in development
- **NOT thread-safe** - for debugging only

**Files:**
- `mutex_teensy.h` - Complete inline implementation

## Platform Support

### Teensy LC (Cortex-M0+)
- **CPU:** MKL26Z64, 48 MHz
- **TeensyThreads:** ✅ Supported
- **FreeRTOS:** ⚠️ Limited (M0+ has reduced FreeRTOS feature set)
- **DMB Support:** ✅ Yes (Cortex-M0+ has DMB instruction)

### Teensy 3.x (Cortex-M4/M4F)
- **CPU:** MK20DX256 (3.1/3.2), MK64FX512 (3.5), MK66FX1M0 (3.6)
- **TeensyThreads:** ✅ Supported
- **FreeRTOS:** ✅ Supported
- **DMB Support:** ✅ Yes

### Teensy 4.x (Cortex-M7)
- **CPU:** IMXRT1062, 600 MHz
- **TeensyThreads:** ✅ Supported
- **FreeRTOS:** ✅ Supported (recommended for 4.x)
- **DMB Support:** ✅ Yes

## Memory Barriers

All Teensy platforms support ARM memory barriers (DMB instruction), including Cortex-M0+ on Teensy LC:

- **Cortex-M0+** (Teensy LC): Has DMB, DSB, ISB instructions
- **Cortex-M4** (Teensy 3.x): Full memory barrier support
- **Cortex-M7** (Teensy 4.x): Full memory barrier support with enhanced features

FastLED's atomic operations automatically use appropriate memory barriers on all Teensy platforms.

## Comparison: FreeRTOS vs TeensyThreads

| Feature | FreeRTOS | TeensyThreads |
|---------|----------|---------------|
| **Scheduling** | Preemptive | Cooperative |
| **Thread Safety** | True concurrent | Yield-based |
| **Overhead** | Higher (scheduler) | Lower |
| **Priority** | Task priorities | No priorities |
| **ISR Safety** | ISR-safe primitives | Not ISR-safe |
| **Recommended For** | Teensy 4.x | Teensy 3.x, simple apps |

## Best Practices

1. **Choose the right library:**
   - FreeRTOS for Teensy 4.x and complex applications
   - TeensyThreads for simpler cooperative threading

2. **Include order is flexible:**
   ```cpp
   #include <Threading_Library.h>  // Can be in any order
   #include <FastLED.h>
   ```
   FastLED automatically detects threading libraries using `__has_include`.

3. **Use RAII lock guards:**
   ```cpp
   fl::unique_lock<fl::MutexTeensy> lock(mutex);  // Automatic unlock
   ```

4. **Avoid long critical sections:**
   - Keep locked code sections short
   - Don't call `FastLED.show()` inside locks if possible

5. **TeensyThreads requires explicit yields:**
   ```cpp
   threads.yield();  // Must yield in cooperative mode
   ```

## Troubleshooting

### "Fake mutex detected but expected real mutex"

**Problem:** Threading library not installed or not in library search path.

**Solution:**
1. Verify the threading library is properly installed via Arduino Library Manager or PlatformIO
2. Check that the library path is in your compiler's include search paths
3. Try a simple test sketch that includes the threading library to verify installation

### "Mutex lock failed"

**Problem:** FreeRTOS mutex creation or locking failed.

**Solution:**
- Check available heap memory (`xPortGetFreeHeapSize()`)
- Ensure FreeRTOS scheduler is running (`vTaskStartScheduler()`)
- Verify task stack sizes are sufficient

### "Unlock called by non-owner thread"

**Problem:** Trying to unlock a recursive mutex from a different thread.

**Solution:** Only the thread that locked the mutex can unlock it. Check your threading logic.

## Technical References

### Detection Macros

```cpp
FASTLED_TEENSY_HAS_FREERTOS  // 1 if FreeRTOS detected, 0 otherwise
FASTLED_TEENSY_HAS_THREADS   // 1 if TeensyThreads detected, 0 otherwise
FASTLED_TEENSY_REAL_MUTEX    // 1 if real mutex available, 0 for fake
```

### Automatic Detection System

FastLED uses C++17's `__has_include` preprocessor feature to automatically detect available threading libraries:

1. **FreeRTOS Detection**: Checks for `<FreeRTOS.h>` or `<Arduino_FreeRTOS_ARM.h>` at compile time
2. **TeensyThreads Detection**: Checks for `<TeensyThreads.h>` at compile time
3. **Include Order Independent**: Libraries are auto-included when detected, so include order doesn't matter
4. **Priority**: FreeRTOS takes priority if both libraries are present (preemptive > cooperative)

This approach eliminates include-order dependencies and makes FastLED easier to use.

## License

This implementation is part of FastLED and follows the FastLED license.

## See Also

- [FreeRTOS Documentation](https://www.freertos.org/Documentation/RTOS_book.html)
- [TeensyThreads GitHub](https://github.com/ftrias/TeensyThreads)
- [FastLED Mutex Documentation](../../../fl/stl/mutex.h)
- [ARM Cortex-M Memory Barriers](https://developer.arm.com/documentation/dui0552/a/the-cortex-m3-processor/memory-model/memory-barriers)
