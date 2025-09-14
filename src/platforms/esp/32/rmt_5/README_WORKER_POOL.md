# RMT5 Worker Pool Implementation

## Overview

The RMT5 Worker Pool is a resource management system that allows FastLED to control more LED strips than the available hardware RMT channels on ESP32 platforms. It provides transparent resource sharing while maintaining backward compatibility and preserving async behavior when possible.

## Hardware Limitations

| ESP32 Variant | RMT Channels Available |
|---------------|----------------------|
| ESP32         | 8                    |
| ESP32-S2/S3   | 4                    |
| ESP32-C3/H2   | 2                    |

## Key Features

### 1. **Transparent Resource Sharing**
- Controllers don't need to know about resource sharing
- Existing FastLED code works without modification
- Worker pool manages RMT channel allocation automatically

### 2. **Async Behavior Preservation**
- When N ≤ K (strips ≤ channels): Full async behavior maintained
- When N > K (strips > channels): First K strips run async, others queue with controlled polling

### 3. **Efficient Buffer Management**
- Each controller maintains its own persistent pixel buffer
- Workers use external buffers to avoid memory copying where possible
- Buffer pooling for optimal memory usage

### 4. **Backward Compatibility**
- Legacy mode available via compile flags
- Existing RmtController5 API unchanged
- Gradual migration path

## Architecture

```
ClocklessController
  └── RmtController5 (maintains pixel buffer)
      └── RmtWorkerPool (singleton)
          └── RmtWorker[] (hardware abstraction)
              └── IRmtStrip (ESP-IDF interface)
                  └── led_strip_handle_t (hardware)
```

## Usage

### Basic Usage (Automatic)
```cpp
// No changes needed - worker pool is enabled by default
FastLED.addLeds<WS2812B, 2, GRB>(leds1, NUM_LEDS);
FastLED.addLeds<WS2812B, 4, GRB>(leds2, NUM_LEDS);
FastLED.addLeds<WS2812B, 5, GRB>(leds3, NUM_LEDS);
// ... add more strips than hardware channels
FastLED.show(); // Worker pool manages everything transparently
```

### Disable Worker Pool (Legacy Mode)
```cpp
// Compile with this flag to disable worker pool globally
#define FASTLED_RMT5_DISABLE_WORKER_POOL
#include <FastLED.h>

// Or force legacy mode at runtime
#define FASTLED_RMT5_FORCE_LEGACY_MODE 1
#include <FastLED.h>
```

## Performance Characteristics

### When N ≤ K (Within Channel Limits)
- **Latency**: Same as legacy mode (no additional overhead)
- **Throughput**: Same as legacy mode
- **Async Behavior**: Fully preserved
- **Memory**: Slightly higher (persistent buffers)

### When N > K (Exceeding Channel Limits)
- **Latency**: First K strips: same as legacy, others: queued with ~1ms polling
- **Throughput**: Limited by RMT transmission time
- **Async Behavior**: Mixed (first K async, others polled)
- **Memory**: Higher (buffers for all controllers)

## Configuration Options

### Compile-Time Flags

```cpp
// Disable worker pool entirely (use legacy mode)
#define FASTLED_RMT5_DISABLE_WORKER_POOL

// Force legacy mode even when worker pool is compiled in
#define FASTLED_RMT5_FORCE_LEGACY_MODE 1
```

### Runtime Configuration
Worker pool is automatically enabled/disabled based on compile flags. No runtime configuration is currently available to maintain API simplicity.

## Implementation Details

### Worker Pool Lifecycle
1. **Initialization**: Creates one worker per hardware RMT channel
2. **Registration**: Controllers register with pool in constructor
3. **Coordination**: `FastLED.show()` triggers coordinated draw cycle
4. **Resource Management**: Workers are assigned/released as needed
5. **Cleanup**: Controllers unregister in destructor

### Draw Cycle Flow
```cpp
FastLED.show()
  └── RmtController5::showPixels()
      └── RmtWorkerPool::executeDrawCycle()
          ├── N ≤ K: executeAsyncOnlyMode() (immediate return)
          └── N > K: executeMixedMode() (polling for queued)
```

### Buffer Management
- **Controller Buffers**: Each controller maintains persistent pixel data
- **Worker Buffers**: Workers use external buffers for ESP-IDF led_strip
- **Buffer Copying**: Required when switching between controllers
- **Buffer Pooling**: Size-based buffer reuse to minimize allocations

### Error Handling
- **Worker Allocation Failures**: Graceful degradation
- **RMT Channel Creation Failures**: Error logging and fallback
- **Buffer Allocation Failures**: Error reporting
- **Transmission Errors**: Proper cleanup and recovery

## Testing

### Unit Tests
```bash
# Run RMT5 worker pool tests
uv run python test.py rmt5_worker_pool
```

### Example Code
```cpp
// See examples/RMT5WorkerPool/RMT5WorkerPool.ino
// Tests multiple strips exceeding hardware limits
```

### Performance Testing
The example includes built-in performance testing:
- `perf` command: Measure FastLED.show() timing
- `independence` command: Verify strip independence
- `info` command: Display system information

## Troubleshooting

### Common Issues

1. **Compilation Errors**
   - Ensure ESP-IDF version supports led_strip with external buffers
   - Check that FASTLED_RMT5 is enabled

2. **Runtime Errors**
   - Verify GPIO pins are valid for RMT usage
   - Check available memory for buffer allocation
   - Ensure LED strip wiring is correct

3. **Performance Issues**
   - Monitor serial output for timing information
   - Consider reducing number of LEDs if memory constrained
   - Use performance test commands in example

### Debug Logging
Enable ESP-IDF logging for detailed information:
```cpp
// In your sketch setup()
esp_log_level_set("rmt_worker_pool", ESP_LOG_DEBUG);
esp_log_level_set("idf5_rmt.cpp", ESP_LOG_DEBUG);
```

## Future Enhancements

### Planned Features
1. **Priority System**: Allow high-priority strips to get workers first
2. **Smart Batching**: Group compatible strips to minimize reconfiguration
3. **Dynamic Scaling**: Adjust worker count based on usage patterns
4. **Metrics**: Performance monitoring and statistics
5. **Runtime Configuration**: Enable/disable worker pool at runtime

### Performance Optimizations
1. **Buffer Pool Optimization**: Advanced buffer size prediction
2. **Worker Affinity**: Prefer workers already configured for similar strips
3. **Completion Prediction**: Estimate transmission completion time
4. **Load Balancing**: Distribute strips across workers optimally

## Contributing

When modifying the worker pool implementation:

1. **Maintain Backward Compatibility**: Existing code must work unchanged
2. **Preserve Async Behavior**: Don't break async performance when N ≤ K
3. **Add Tests**: Include unit tests for new functionality
4. **Update Documentation**: Keep this README current
5. **Performance Testing**: Verify no regression in timing-critical paths

## References

- [ESP-IDF LED Strip Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/led_strip.html)
- [ESP32 RMT Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/rmt.html)
- [FastLED RMT4 Implementation](../rmt/) (reference for coordination patterns)
- [Original Design Document](../../../../FEATURE_RMT5_POOL.md)