# RMT RX Driver - Receive Mode for ESP32 RMT Peripheral

## Overview

This driver provides RMT (Remote Control) receive functionality for ESP32 platforms. It enables capturing and decoding timing signals from external sources, particularly useful for validating LED strip output by reading back transmitted signals.

**Primary Use Case**: Hardware-in-the-loop validation of SPI/RMT TX LED output

## Architecture

```
GPIO Pin → RMT RX Peripheral → Symbol Buffer → Decoder → Byte Array
```

The driver provides:

1. **RmtRxChannel** - RX channel with integrated decoder
   - Hardware configuration and lifecycle management
   - Non-blocking receive operations with `wait()`
   - ISR-based completion notification
   - Built-in symbol-to-byte decoding via `decode()` method
   - Timing threshold detection for protocol-specific decoding (WS2812B, etc.)

## Quick Start

### Basic Usage (Recommended API)

```cpp
#include "platforms/esp/32/drivers/rmt_rx/rmt_rx_channel.h"

// Create RX channel (GPIO 6, 40MHz resolution, 1024 symbol buffer)
auto rx = fl::esp32::RmtRxChannel::create(6, 40000000, 1024);

// Initialize hardware
if (!rx->begin()) {
    FL_WARN("RX channel init failed");
    return;
}

// Wait for data (50ms timeout)
if (rx->wait(50) == fl::esp32::RmtRxWaitResult::SUCCESS) {
    // Access received symbols
    auto symbols = rx->getReceivedSymbols();
    FL_DBG("Received " << symbols.size() << " symbols");

    // Decode to bytes
    fl::vector<uint8_t> bytes;
    auto result = rx->decode(fl::esp32::CHIPSET_TIMING_WS2812B_RX,
                             fl::back_inserter(bytes));
    if (result.ok()) {
        FL_DBG("Decoded " << result.value() << " bytes");
    }
}
```

### Loopback Validation Example

```cpp
// TX setup (existing FastLED)
#define TX_PIN 5
CRGB leds[10];
FastLED.addLeds<WS2812B, TX_PIN, GRB>(leds, 10);

// RX setup (use same pin with io_loop_back)
auto rx = fl::esp32::RmtRxChannel::create(TX_PIN, 40000000, 1024);
rx->begin();

// Test pattern
fill_solid(leds, 10, CRGB::Red);

// Manual control API (for precise timing)
RmtSymbol rx_symbols[1024];
rx->clear();
rx->startReceive(rx_symbols, 1024);

// Transmit
FastLED.show();

// Wait for RX completion
while (!rx->finished()) {
    delayMicroseconds(10);
}

// Decode and validate
fl::vector<uint8_t> bytes;
auto result = rx->decode(fl::esp32::CHIPSET_TIMING_WS2812B_RX,
                         fl::back_inserter(bytes));
// Compare bytes against expected values...
```

## Hardware Requirements

### Wiring for Loopback Testing

```
ESP32 DevKit
┌─────────────┐
│ GPIO 5 (TX) ├──┬── Jumper Wire ──┐
│             │  │                  │
│ GPIO 6 (RX) ├──┘                  │
│             │                     │
│ GND         ├──── LED Strip GND   │
└─────────────┘                     │
                                    │
LED Strip (WS2812B)                 │
┌─────────────┐                     │
│ DIN         ├─────────────────────┘
│ GND         ├──── ESP32 GND
│ +5V         ├──── External Power
└─────────────┘
```

**Minimal Setup** (TX→RX only):
- Jumper wire from GPIO 5 → GPIO 6
- No LED strip needed for basic testing

**Full Setup** (with LED strip):
- TX pin → LED strip DIN
- Loopback wire from TX pin → RX pin
- LED strip GND → ESP32 GND
- LED strip +5V → External power supply

### GPIO Pin Selection

**Constraints**:
- RX pin must support input mode
- Avoid strapping pins (GPIO 0, 2, 12, 15 on ESP32)
- Avoid flash pins (GPIO 6-11 on ESP32, varies by model)

**Recommended Pins**:
- ESP32: GPIO 16-19, 21-23, 25-27, 32-33
- ESP32-S3: GPIO 1-21, 35-48 (avoid USB pins 19-20)
- ESP32-C3: GPIO 2-10, 18-21

## API Reference

### RmtRxChannel Class

#### Constructor
```cpp
RmtRxChannel(gpio_num_t pin, uint32_t resolution_hz = 40000000)
```
- **pin**: GPIO pin for receiving signals
- **resolution_hz**: RMT clock resolution (default 40MHz = 25ns per tick)

#### Methods

**`bool begin()`**
- Initialize RMT RX channel and register ISR callback
- Returns `true` on success, `false` on failure
- Must be called before `startReceive()`

**`bool startReceive(rmt_symbol_word_t* buffer, size_t buffer_size)`**
- Start non-blocking receive operation
- **buffer**: Caller-provided buffer for symbols (must remain valid until done)
- **buffer_size**: Number of symbols buffer can hold
- Returns `true` if receive started, `false` on error

**`bool isReceiveDone() const`**
- Check if receive operation completed
- Returns `true` when ISR callback fires (receive done)

**`size_t getReceivedSymbols() const`**
- Get number of symbols received
- Valid after `isReceiveDone()` returns `true`

**`void reset()`**
- Reset receive state for new operation
- Clears `receive_done_` and `symbols_received_` flags

### RMT Symbol Format

```cpp
typedef struct {
    uint32_t duration0 : 15;  // Duration of level0 (in ticks)
    uint32_t level0    : 1;   // Level of first half (0 or 1)
    uint32_t duration1 : 15;  // Duration of level1 (in ticks)
    uint32_t level1    : 1;   // Level of second half (0 or 1)
} rmt_symbol_word_t;
```

**Example** (WS2812B bit 0 at 40MHz):
```
Bit 0: T0H=400ns, T0L=850ns
Symbol: {duration0=16, level0=1, duration1=34, level1=0}
         16 ticks × 25ns = 400ns high
         34 ticks × 25ns = 850ns low
```

## Buffer Sizing

### Calculating Buffer Size

For LED protocols transmitting 24 bits (3 bytes) per pixel:

```
Symbols per LED = 24 bits × 1 symbol/bit = 24 symbols
Symbols for N LEDs = N × 24 symbols
Add 20% margin for reset pulses and overhead

Example (10 LEDs):
  Minimum: 10 × 24 = 240 symbols
  Recommended: 240 × 1.2 = 288 symbols
  Rounded up: 300 symbols
```

### Memory Usage

```cpp
sizeof(rmt_symbol_word_t) = 4 bytes

Buffer memory = buffer_size × 4 bytes

Examples:
  256 symbols = 1 KB
  512 symbols = 2 KB
  1024 symbols = 4 KB
```

## Timing Configuration

### Resolution Selection

**40MHz (default)** - 25ns per tick
- Matches TX encoder default
- Sufficient for WS2812B (400ns/800ns transitions)
- Maximum duration per half-symbol: 32767 ticks × 25ns = 819.175μs

**10MHz** - 100ns per tick
- Lower precision, longer maximum duration
- Maximum duration: 32767 ticks × 100ns = 3.2767ms
- Useful for slow protocols (e.g., IR remote control)

**80MHz** - 12.5ns per tick
- Higher precision for tight timing tolerances
- Maximum duration: 32767 ticks × 12.5ns = 409.588μs
- May not support long reset pulses

### Signal Range Parameters

```cpp
rmt_receive_config_t rx_params = {};
rx_params.signal_range_min_ns = 100;      // Min pulse width: 100ns
rx_params.signal_range_max_ns = 100000000; // Max pulse width: 100ms
```

- **signal_range_min_ns**: Pulses shorter than this are ignored (noise filtering)
- **signal_range_max_ns**: Pulses longer than this terminate reception

## Platform Support

| Platform | RMT RX | DMA Support | Tested |
|----------|--------|-------------|--------|
| ESP32 (classic) | ✅ Yes | ❌ No | ⚠️ Pending |
| ESP32-S2 | ✅ Yes | ❌ No | ⚠️ Pending |
| ESP32-S3 | ✅ Yes | ✅ Yes* | ⚠️ Pending |
| ESP32-C3 | ✅ Yes | ❌ No | ⚠️ Pending |
| ESP32-C6 | ✅ Yes | ❌ No | ⚠️ Pending |
| ESP32-H2 | ✅ Yes | ❌ No | ⚠️ Pending |
| ESP32-P4 | ✅ Yes | ✅ Yes* | ⚠️ Pending |

*DMA support is available but not yet enabled in this driver (v1.0 uses non-DMA mode for universal compatibility)

## Performance Characteristics

### Non-DMA Mode (Current Implementation)

- **Maximum symbols per receive**: Limited by buffer size
- **CPU overhead**: Low (ISR callback only)
- **Latency**: ~1-10μs from signal end to ISR callback
- **Typical use case**: Validation sketches (10-100 LEDs)

### DMA Mode (Future Enhancement)

- **Maximum symbols per receive**: 4095 per DMA transaction
- **CPU overhead**: Minimal (DMA handles data transfer)
- **Latency**: Similar to non-DMA
- **Typical use case**: Large LED arrays (>100 LEDs)

## Error Handling

### Common Errors and Solutions

**Error: "RX channel not initialized"**
- **Cause**: Called `startReceive()` before `begin()`
- **Solution**: Call `begin()` first and check return value

**Error: "Failed to create RX channel"**
- **Cause**: Invalid GPIO pin or RMT peripheral unavailable
- **Solution**: Check pin selection and verify RMT not already in use

**Error: Receive never completes (isReceiveDone() stays false)**
- **Cause**: No signal on RX pin, or signal outside range
- **Solution**: Verify wiring, check TX is transmitting, adjust signal_range_*

**Error: Buffer overflow (symbols truncated)**
- **Cause**: Buffer too small for transmission
- **Solution**: Increase buffer size (see Buffer Sizing section)

## Debug Logging

Enable FastLED debug logging to troubleshoot RX issues:

```cpp
// In your sketch before any FastLED includes
#define FASTLED_DEBUG 1

#include <FastLED.h>
```

**Example debug output**:
```
RmtRxChannel constructed: pin=6 resolution=40000000Hz
RX channel created successfully
RX callbacks registered successfully
RX channel enabled
RX receive started (buffer size: 1024 symbols)
```

## Implementation Notes

### ISR Safety

The receive callback runs in ISR context:
- Uses `IRAM_ATTR` to ensure code is in IRAM
- Updates only volatile flags (no complex operations)
- No heap allocation or blocking calls

### Thread Safety

The `RmtRxChannel` class is **not thread-safe**:
- Do not call methods from multiple threads
- Do not call methods from ISR context (except the internal callback)
- Use external synchronization if needed

### Resource Cleanup

The destructor automatically releases the RMT channel:
```cpp
{
    RmtRxChannel rx(GPIO_NUM_6);
    rx.begin();
    // Use channel...
}  // Channel automatically released here
```

## Future Enhancements

### Implemented in Agent 3 (decodeRmtSymbols)

- ✅ Symbol-to-byte decoder with timing threshold detection
- ✅ Support for WS2812B LED protocol
- ✅ Error counting and quality metrics
- ✅ Reset pulse detection for frame boundaries

### Planned for Future Agents

- Support for additional LED protocols (APA102, SK6812, etc.)
- Automatic protocol detection based on timing analysis

### Planned for Agent 6 (Integration)

- DMA mode support for high-throughput applications
- Continuous receive mode (circular buffer)
- Hardware filtering for noise rejection
- Performance benchmarking and optimization

## Related Documentation

- `docs/RMT_RX_ARCHITECTURE.md` - System architecture and design
- `examples/Validation/README.md` - Loopback validation example
- `src/platforms/esp/32/drivers/rmt/rmt_5/README.md` - RMT TX encoder documentation
- ESP-IDF RMT documentation: https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/peripherals/rmt.html

## Version History

**v1.0 (Agent 2)** - Initial implementation
- RmtRxChannel class with non-DMA mode
- ISR-based completion notification
- Basic error handling
- Platform compatibility (ESP32/S2/S3/C3/C6/H2/P4)

**v2.0 (Agent 3)** - Decoder implementation
- decodeRmtSymbols() function-based decoder
- WS2812B timing thresholds
- Symbol-to-byte conversion with error reporting

## License

FastLED library - MIT License
