# ESP32-S3 LCD/I80 Parallel LED Driver Demo

This example demonstrates the ESP32-S3 LCD/I80 parallel LED driver with 50ns timing resolution and multi-chipset support. It can drive up to 16 WS28xx LED strips simultaneously with precise timing control.

## Features

- **Multi-chipset Support**: Mix WS2812, WS2816, WS2813, and other WS28xx variants in the same frame
- **High Performance**: 50ns timing resolution (20MHz PCLK) with double-buffered PSRAM operation
- **16 Parallel Lanes**: Drive up to 16 LED strips simultaneously
- **Precise Timing**: Uses FastLED's canonical timing definitions from chipsets.h
- **Real-time Monitoring**: Performance and memory usage statistics

## Hardware Requirements

### ESP32-S3 Board
- **Required**: ESP32-S3 with PSRAM (8MB recommended)
- **Tested on**: ESP32-S3-DevKitC-1, Seeed XIAO ESP32S3

### LED Strips
- **Supported**: WS2812, WS2812B, WS2816, WS2813, WS2811, and compatible chipsets
- **Configuration**: Up to 16 strips, 300 LEDs per strip (configurable)
- **Power**: Ensure adequate 5V power supply for your LED count

### GPIO Connections

| Lane | GPIO | Chipset | Description |
|------|------|---------|-------------|
| 0-7  | 1-8  | WS2812  | First 8 lanes with WS2812 timing |
| 8-15 | 9-16 | WS2816  | Last 8 lanes with WS2816 timing |

**WR (Clock) Pin**: GPIO 47 (automatically configured)

> **Note**: GPIO assignments can be modified in the source code. Avoid pins used by USB, UART, or other peripherals.

## Software Requirements

### PlatformIO
```ini
platform = espressif32@^6.0.0
framework = arduino
board = esp32s3dev
```

### Arduino IDE
- **ESP32 Arduino Core**: 2.0.9 or later
- **Board**: ESP32S3 Dev Module
- **PSRAM**: Enabled (OPI PSRAM)
- **Flash Size**: 16MB
- **Partition Scheme**: Huge APP (3MB No OTA/1MB SPIFFS)

## Configuration

### Memory Requirements
- **PSRAM**: Required for DMA buffers
- **Buffer Size**: ~1.2MB per 1000 LEDs (double-buffered)
- **Example**: 16 strips × 300 LEDs = ~360KB per buffer, ~720KB total

### Timing Configuration
```cpp
// Default: 20MHz PCLK = 50ns slots
config.pclk_hz = 20000000;

// Latch gap: 300µs (standard reset time)
config.latch_us = 300;
```

### Chipset Mixing
```cpp
// Lanes 0-7: WS2812 timing
config.lanes.push_back(LaneConfig(GPIO_1, LedChipset::WS2812));

// Lanes 8-15: WS2816 timing  
config.lanes.push_back(LaneConfig(GPIO_9, LedChipset::WS2816));

// Custom timing example
config.lanes.push_back(LaneConfig(GPIO_10, 300, 600, 400)); // T1, T2, T3 in ns
```

## Demo Patterns

The demo cycles through four patterns every 10 seconds:

1. **Rainbow**: Multi-hue rainbow with phase offset per strip
2. **Gradient**: Red-to-blue gradient across strips with brightness variation
3. **Binary Test**: Alternating bit patterns for timing validation
4. **Chipset Demo**: Different colors for WS2812 vs WS2816 lanes

## Performance

### Theoretical Limits
- **Frame Rate**: ~100 FPS for 300 LEDs/strip, ~31 FPS for 1000 LEDs/strip
- **Memory Bandwidth**: 20 MB/s DMA throughput
- **CPU Usage**: ~10-15% for encoding at maximum frame rates

### Monitoring
The demo displays real-time performance metrics:
```
Performance: 87.3 FPS (89.2% of max), Free heap: 234560 bytes
```

## Troubleshooting

### Common Issues

**"PSRAM not detected!"**
- Enable PSRAM in board configuration
- Verify board has PSRAM installed
- Check PSRAM type setting (OPI PSRAM for most S3 boards)

**"Failed to initialize I80 bus"**
- Check GPIO pin conflicts
- Verify ESP-IDF version compatibility
- Ensure adequate power supply

**Poor performance/low FPS**
- Disable USB-CDC console (use UART)
- Reduce LED count or strip count
- Check for memory fragmentation
- Disable WiFi/Bluetooth if enabled

**LED timing issues**
- Verify chipset selection matches your LEDs
- Use logic analyzer to validate timing
- Adjust PCLK frequency if needed
- Check power supply stability

### Logic Analyzer Verification

To verify timing accuracy:
1. Connect logic analyzer to data pins
2. Capture during binary test pattern
3. Measure T1 (high time for bit 0), T1+T2 (high time for bit 1)
4. Verify total bit time matches expected value

**Expected Timing (50ns slots)**:
- WS2812: T1=250ns (5 slots), T2=625ns (12-13 slots), T3=375ns (7-8 slots)
- Total bit time: ~1.25-1.30µs (25-26 slots)

## Advanced Usage

### Custom Chipset Timing
```cpp
// Define custom timing for specific LED variant
driver.setLaneTimings(0, 300, 700, 350);  // T1, T2, T3 in ns
```

### Runtime Chipset Changes
```cpp
// Change chipset for lane 0 between frames
driver.setLaneChipset(0, LedChipset::WS2813);
```

### Memory Optimization
```cpp
// Use internal RAM for smaller configurations
config.use_psram = false;  // For < 100 LEDs per strip
```

## Design Documentation

For detailed technical information, see:
- **Design Document**: `src/third_party/yves/esp32s3_lcd_parallel_design.md`
- **Driver Implementation**: `src/platforms/esp/32/esp32s3_clockless_i2s.h`

## License

This example is part of the FastLED library and follows the same licensing terms.

## Contributing

Found a bug or have an improvement? Please contribute to the FastLED project on GitHub.