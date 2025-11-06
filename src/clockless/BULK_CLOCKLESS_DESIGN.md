# Bulk Clockless Controller Architecture

## Overview

The Bulk Clockless Controller is a high-performance LED driver architecture that batches multiple LED strips with the same chipset timing into a single hardware transmission. This design is inspired by the ObjectFLED driver for Teensy 4.x and represents an ideal pattern for bulk controller implementations across different platforms.

**Key Benefits**:
- **Efficient Batching**: Multiple strips transmitted together using shared hardware resources
- **Zero Extra Copies**: Pixels written directly to driver's frame buffer
- **Generic Chipset Support**: Works with any clockless LED chipset (WS2812, SK6812, WS2811, etc.)
- **Deferred Rendering**: Strips queued during `FastLED.show()`, transmitted as a batch

---

## Architecture Components

### 1. **Hardware Driver Layer**

The low-level driver that manages platform-specific hardware (DMA, timers, GPIO).

**Responsibilities**:
- Hardware initialization (timers, DMA, GPIO configuration)
- Bit encoding (convert RGB/RGBW bytes to hardware-specific format)
- Timing control (T0H, T1H, period configuration)
- Asynchronous transmission (DMA-driven or interrupt-driven)

**Example**: `ObjectFLED` class on Teensy 4.x

**Key Methods**:
```cpp
void begin(uint16_t t1, uint16_t t2, uint16_t t3, uint16_t latch_delay_us)
void show()  // Trigger transmission
bool busy()  // Check if transmission in progress
```

---

### 2. **Hardware Resource Manager (Singleton)**

Ensures exclusive access to shared hardware resources that can only handle one transmission at a time.

**Shared Resources** (platform-specific):
- DMA channels
- Timer peripherals
- Shared memory buffers (DMAMEM, SRAM, etc.)

**Key Methods**:
```cpp
void acquire(void* owner)           // Wait for hardware, take ownership
void release(void* owner)           // Release ownership (transmission continues)
void waitForCompletion()            // Block until transmission done
bool isBusy()                       // Check transmission status
void* getCurrentOwner()             // Get current owner
```

**Pattern**: Blocking acquire + non-blocking release
```cpp
manager.acquire(this);        // Wait for previous transmission
// Configure hardware
// Start transmission
manager.release(this);        // Return immediately (async transmission)
```

---

### 3. **Chipset Grouping System**

One driver instance per unique chipset timing configuration.

#### ObjectFLEDGroupBase (Concrete Implementation)

**Responsibilities**:
- Track strip configuration (pins, lengths, RGB/RGBW)
- Detect configuration changes (trigger driver rebuild)
- Manage frame buffer layout (interleaved strips)
- Write pixel data directly to driver's frame buffer
- Coordinate transmission via driver's `show()` method

**Key Data Structures**:
```cpp
struct StripInfo {
    uint8_t pin;
    uint16_t numLeds;
    uint16_t numBytes;       // numLeds * (3 or 4)
    uint16_t offsetBytes;    // Offset into frame buffer
    bool isRgbw;
};

vector<StripInfo> strips;         // Current frame's strips
vector<StripInfo> prevStrips;     // Previous frame (for change detection)
```

**Key Methods**:
```cpp
void onQueuingStart()                          // Clear strip list, start new frame
void addStrip(uint8_t pin, PixelIterator& it) // Add strip + write pixels
void flush()                                   // Transmit if dirty
```

#### ObjectFLEDGroup<TIMING> (Template Wrapper)

**Purpose**: Convert compile-time `TIMING` struct to runtime configuration.

```cpp
template <typename TIMING>
class ObjectFLEDGroup {
public:
    static ObjectFLEDGroup& getInstance() {
        return Singleton<ObjectFLEDGroup<TIMING>>::instance();
    }

    ObjectFLEDGroup()
        : mBase(ObjectFLEDTimingConfig{
            TIMING::T1, TIMING::T2, TIMING::T3, TIMING::RESET
          }) {
        // Auto-register with global registry
        ObjectFLEDRegistry::getInstance().registerGroup(this, flushFunc);
    }

private:
    ObjectFLEDGroupBase mBase;  // Delegate all work to concrete base
};
```

**Result**: One singleton per chipset type:
- `ObjectFLEDGroup<TIMING_WS2812_800KHZ>`
- `ObjectFLEDGroup<TIMING_SK6812>`
- `ObjectFLEDGroup<TIMING_WS2811>`

---

### 4. **Global Registry**

Tracks all active chipset groups and coordinates flushing.

**Purpose**: Ensure only one chipset type transmits at a time.

**Key Methods**:
```cpp
void registerGroup(void* groupPtr, void (*flushFunc)(void*))
void flushAll()              // Flush all groups
void flushAllExcept(void* exceptPtr)  // Flush all except one
```

**Usage**:
```cpp
// Before queuing strips of a new chipset type:
Registry::getInstance().flushAllExcept(currentGroup);
```

---

### 5. **FastLED Proxy Controller**

Integrates the bulk driver into FastLED's controller architecture.

**Template Signature**:
```cpp
template <typename TIMING, int DATA_PIN, EOrder RGB_ORDER>
class ClocklessController_Bulk_Proxy : public CPixelLEDController<RGB_ORDER>
```

**Key Methods**:
```cpp
void* beginShowLeds(int nleds) override {
    void* data = Base::beginShowLeds(nleds);

    // Get singleton for THIS chipset type
    auto& group = ObjectFLEDGroup<TIMING>::getInstance();

    // Flush any pending groups of DIFFERENT chipset types
    ObjectFLEDRegistry::getInstance().flushAllExcept(&group);

    // Start queuing for this group
    group.onQueuingStart();

    return data;
}

void showPixels(PixelController<RGB_ORDER>& pixels) override {
    // Get singleton for THIS chipset type
    auto& group = ObjectFLEDGroup<TIMING>::getInstance();

    // Add this strip to the group (writes pixels immediately)
    auto pixel_iterator = pixels.as_iterator(this->getRgbw());
    group.addStrip(DATA_PIN, pixel_iterator);
}

void endShowLeds(void* data) override {
    Base::endShowLeds(data);

    // DON'T flush here - let chipset change or frame end handle it
    // Flushing happens in next controller's beginShowLeds() or FastLED.show() end
}
```

---

## Data Flow

### Complete Show Process

```
1. USER CALLS FastLED.show()
   ↓
2. FOR EACH STRIP (in registration order):

   Strip A (WS2812, Pin 5):
   beginShowLeds(100)
   → Group<WS2812> = getInstance()
   → Registry.flushAllExcept(Group<WS2812>)  // Flush other chipsets
   → Group.onQueuingStart()                   // Clear strip list

   showPixels(pixels)
   → Group.addStrip(pin=5, pixels)           // Add to list
     → writePixels()                         // Copy to frameBuffer

   endShowLeds()
   → (no action)

   ↓
   Strip B (WS2812, Pin 6):
   beginShowLeds(100)
   → Group<WS2812> = getInstance()           // Same group!
   → Registry.flushAllExcept(Group<WS2812>)  // No-op (already active)
   → Group.onQueuingStart()                   // No-op (already queuing)

   showPixels(pixels)
   → Group.addStrip(pin=6, pixels)           // Add second strip

   endShowLeds()
   → (no action)

   ↓
   Strip C (SK6812, Pin 7):
   beginShowLeds(50)
   → Group<SK6812> = getInstance()           // Different group!
   → Registry.flushAllExcept(Group<SK6812>)  // FLUSH Group<WS2812>
     → Group<WS2812>.flush()
       → Driver.show()                       // TRANSMIT strips A+B
   → Group.onQueuingStart()                   // Start queuing SK6812

   showPixels(pixels)
   → Group.addStrip(pin=7, pixels)

   endShowLeds()
   → (no action)

3. END OF FastLED.show()
   → Registry.flushAll()                      // FLUSH remaining groups
     → Group<SK6812>.flush()
       → Driver.show()                        // TRANSMIT strip C
```

**Key Insight**: Strips are **batched by chipset type**, transmitted when chipset changes or frame ends.

---

## Memory Layout

### Interleaved Strip Layout

Strips are laid out in memory with padding to the maximum strip length:

```
Frame Buffer:
[Strip 0: LED0 LED1 ... LEDn] [padding to maxBytes]
[Strip 1: LED0 LED1 ... LEDn] [padding to maxBytes]
[Strip 2: LED0 LED1 ... LEDn] [padding to maxBytes]
...
```

**Calculation**:
```cpp
// Find longest strip
mMaxBytesPerStrip = 0;
for (auto& strip : strips) {
    if (strip.numBytes > mMaxBytesPerStrip)
        mMaxBytesPerStrip = strip.numBytes;
}

// Assign offsets
uint16_t offset = 0;
for (auto& strip : strips) {
    strip.offsetBytes = offset;
    offset += mMaxBytesPerStrip;
}

// Total memory needed
int totalBytes = mMaxBytesPerStrip * strips.size();
```

**Advantages**:
- Efficient memory access patterns for DMA/ISR
- Simple offset calculation: `base + strip_index * maxBytes`
- Padding ensures alignment for hardware transfers

---

## Configuration Change Detection

Driver rebuilding is expensive, so detect when it's truly necessary:

```cpp
void onQueuingDone() {
    // Check if strip configuration changed
    mStripsChanged = (strips.size() != prevStrips.size());
    if (!mStripsChanged) {
        for (size_t i = 0; i < strips.size(); i++) {
            if (strips[i].pin != prevStrips[i].pin ||
                strips[i].numLeds != prevStrips[i].numLeds ||
                strips[i].isRgbw != prevStrips[i].isRgbw) {
                mStripsChanged = true;
                break;
            }
        }
    }
}

void flush() {
    if (mDrawn || strips.size() == 0)
        return;  // Already drawn or no data

    mDrawn = true;

    if (mStripsChanged || !mDriver) {
        rebuildDriver();  // Only rebuild if needed
    }

    mDriver->show();  // Transmit
}
```

**Triggers for Rebuild**:
- Number of strips changed
- Any pin changed
- Any strip length changed
- RGB/RGBW mode changed

---

## Timing Configuration

### Three-Phase Timing Format

Clockless LEDs use three timing phases per bit:

```
T1: High time for '0' bit
T2: Additional high time for '1' bit
T3: Low time after bit
```

**Examples**:
```cpp
// WS2812B (800 KHz)
T1 = 400ns   (T0H: high time for '0')
T2 = 450ns   (T1H - T0H: additional high for '1')
T3 = 400ns   (low time)
Total = 1250ns (800 KHz)

// WS2811 (400 KHz)
T1 = 500ns
T2 = 1700ns
T3 = 300ns
Total = 2500ns (400 KHz)
```

### Converting to Driver Format

Drivers may use different internal formats:

```cpp
void begin(uint16_t t1, uint16_t t2, uint16_t t3, uint16_t latch_delay) {
    // Convert 3-phase timing to driver's internal format
    // T1 = T0H (high time for '0' bit)
    // T1 + T2 = T1H (high time for '1' bit)
    // T1 + T2 + T3 = total period
    uint16_t clk_ns = t1 + t2 + t3;
    uint16_t t0h_ns = t1;
    uint16_t t1h_ns = t1 + t2;

    configureHardware(clk_ns, t0h_ns, t1h_ns, latch_delay);
}
```

---

## Implementation Checklist

### Required Components

- [ ] **Hardware Driver Class**
  - [ ] `begin(t1, t2, t3, latch_delay)` - Configure timing
  - [ ] `show()` - Trigger transmission
  - [ ] `busy()` / `waitForDmaToFinish()` - Check status
  - [ ] Frame buffer management (`frameBufferLocal`)

- [ ] **Singleton Resource Manager**
  - [ ] `acquire(owner)` - Blocking acquire
  - [ ] `release(owner)` - Non-blocking release
  - [ ] `waitForCompletion()` - Wait for hardware
  - [ ] `isBusy()` - Status check

- [ ] **Concrete Group Class** (`GroupBase`)
  - [ ] Strip tracking (`vector<StripInfo>`)
  - [ ] Configuration change detection
  - [ ] `onQueuingStart()` / `addStrip()` / `flush()`
  - [ ] `rebuildDriver()` - Create driver instance
  - [ ] `writePixels()` - Direct frame buffer writes

- [ ] **Template Group Wrapper** (`Group<TIMING>`)
  - [ ] Singleton per chipset type
  - [ ] Convert `TIMING` struct to runtime config
  - [ ] Auto-register with global registry
  - [ ] Delegate to `GroupBase`

- [ ] **Global Registry**
  - [ ] Track active groups
  - [ ] `flushAll()` / `flushAllExcept()`

- [ ] **FastLED Proxy Controller**
  - [ ] `beginShowLeds()` - Flush other chipsets, start queuing
  - [ ] `showPixels()` - Add strip to group
  - [ ] `endShowLeds()` - No-op (deferred flushing)

---

## Platform-Specific Considerations

### DMA vs. Interrupt-Driven

**DMA-based** (Teensy 4.x, ESP32, STM32):
- Hardware handles bit output
- ISR only refills buffer
- Best performance

**Interrupt-driven** (AVR, ESP8266):
- ISR bit-bangs GPIO per bit
- Higher CPU usage
- Bulk controller still beneficial (shared ISR setup)

### Memory Constraints

**Low RAM platforms** (AVR):
- Consider double-buffering instead of full frame buffer
- Limit maximum strips per group
- Use PROGMEM for timing tables

**High RAM platforms** (ESP32, Teensy):
- Full frame buffer per group
- Support many strips per group

### Pin Validation

Validate pins before adding to driver:
```cpp
auto validation = validate_platform_pin(pin);
if (!validation.valid) {
    FL_WARN("Pin " << pin << " is invalid: " << validation.error_message);
    return;  // Skip this strip
}
```

Platform-specific checks:
- GPIO capability (output mode supported)
- Hardware conflicts (SPI, UART, etc.)
- Voltage level compatibility

---

## Performance Benefits

### vs. Individual Controllers

**Without Bulk Controller**:
```
Strip A: Setup hardware → Transmit → Wait → Teardown
Strip B: Setup hardware → Transmit → Wait → Teardown
Strip C: Setup hardware → Transmit → Wait → Teardown
```

**With Bulk Controller**:
```
Strips A+B+C: Setup hardware → Transmit all → Wait → Teardown
```

**Savings**:
- Reduced hardware reconfigurations
- Fewer context switches
- Better cache utilization
- Lower interrupt overhead

### Parallel vs. Sequential

**Truly Parallel** (like ObjectFLED on Teensy 4.x):
- Multiple strips transmitted simultaneously via DMA
- Limited by hardware (GPIO ports, DMA channels)
- Best case: 42 strips in same time as 1 strip

**Sequential with Shared Setup** (typical implementation):
- Strips transmitted one after another
- Shared hardware setup amortized across strips
- Still significant savings over individual controllers

---

## Example Usage

### User Code (Arduino Sketch)

```cpp
#include <FastLED.h>

#define NUM_LEDS_A 100
#define NUM_LEDS_B 150
#define NUM_LEDS_C 200

CRGB leds_a[NUM_LEDS_A];
CRGB leds_b[NUM_LEDS_B];
CRGB leds_c[NUM_LEDS_C];

void setup() {
    // All three strips use WS2812 timing → batched together
    FastLED.addLeds<WS2812, 5, GRB>(leds_a, NUM_LEDS_A);
    FastLED.addLeds<WS2812, 6, GRB>(leds_b, NUM_LEDS_B);
    FastLED.addLeds<WS2812, 7, GRB>(leds_c, NUM_LEDS_C);
}

void loop() {
    // Modify pixel data
    fill_rainbow(leds_a, NUM_LEDS_A, millis() / 10);
    fill_solid(leds_b, NUM_LEDS_B, CRGB::Blue);
    fadeToBlackBy(leds_c, NUM_LEDS_C, 10);

    // Single show() call transmits all three strips as a batch
    FastLED.show();

    delay(20);
}
```

**What Happens Internally**:
1. All three controllers share `ObjectFLEDGroup<TIMING_WS2812_800KHZ>`
2. First controller's `beginShowLeds()` starts queuing
3. All three `showPixels()` calls add strips to the group
4. After all controllers finish, `Registry::flushAll()` transmits the batch

**Result**: Three strips transmitted together with minimal overhead.

---

## Future Extensions

### Multi-Core Support

For platforms with multiple cores (ESP32):
```cpp
// Core 0: Queuing strips
void loop() {
    updatePixels();
    FastLED.show();  // Returns immediately after queuing
}

// Core 1: Transmission (dedicated)
void transmitTask() {
    while (true) {
        Registry::waitForWork();
        Registry::flushAll();  // Transmit on dedicated core
    }
}
```

### Priority Scheduling

Support high-priority strips:
```cpp
group.addStrip(pin, pixels, priority=HIGH);
// Flush high-priority groups first
```

### Interrupt Budget

Limit time spent in ISR:
```cpp
manager.setInterruptBudgetMicros(100);  // Max 100µs per ISR
// If budget exceeded, yield and resume next frame
```

---

## References

### ObjectFLED (Reference Implementation)
- `src/third_party/object_fled/src/ObjectFLED.{h,cpp}` - Hardware driver
- `src/third_party/object_fled/src/ObjectFLEDDmaManager.{h,cpp}` - Resource manager
- `src/platforms/arm/teensy/teensy4_common/clockless_objectfled.{h,cpp}` - Proxy + grouping

### Timing Definitions
- `src/clockless/timing.h` - Chipset timing structs (T1, T2, T3 format)

### FastLED Controller Base Classes
- `src/cpixel_ledcontroller.h` - `CPixelLEDController` base class
- `src/pixel_iterator.h` - `PixelIterator` for efficient pixel traversal

---

## Summary

The Bulk Clockless Controller architecture provides:

✅ **Efficient Batching** - Multiple strips of the same chipset transmitted together
✅ **Generic Chipsets** - Works with any clockless LED timing (WS2812, SK6812, etc.)
✅ **Zero-Copy Design** - Pixels written directly to driver's frame buffer
✅ **Deferred Rendering** - Strips queued during show(), transmitted at optimal times
✅ **Hardware Sharing** - Singleton manager ensures exclusive hardware access
✅ **Automatic Flushing** - Registry coordinates chipset changes and frame boundaries

This design represents the ideal pattern for high-performance bulk LED controllers across all platforms that support shared hardware resources for clockless LED protocols.
