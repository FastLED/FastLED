# FastLED Codec Examples

This directory contains examples demonstrating the FastLED codec module functionality for JPEG and MPEG1 decoding.

## Overview

The FastLED codec module provides video and image decoding capabilities optimized for LED display applications through individual codec headers (`fl/codec/jpeg.h` and `fl/codec/mpeg1.h`). It supports:

- **JPEG image decoding** for static displays
- **MPEG1 video decoding** for animated content
- **Hardware acceleration** where supported (ESP32-P4, STM32H7)
- **Efficient software fallbacks** for broad platform compatibility
- **Memory-optimized streaming** for resource-constrained devices

## Platform Support

| Platform | JPEG Support | MPEG1 Support | Hardware Acceleration |
|----------|-------------|---------------|----------------------|
| ESP32 | ✓ (ROM decoder) | ✓ (Software) | ROM TJpgDec |
| ESP32-S3 | ✓ (ROM + PIE) | ✓ (Software) | ROM TJpgDec + PIE |
| ESP32-C3/C6 | ✓ (ROM decoder) | ✓ (Software) | ROM TJpgDec |
| ESP32-P4 | ✓ (Hardware) | ✓ (Software) | Hardware JPEG codec |
| Teensy 4.x | ✓ (Software) | ✓ (Software) | DSP instructions |
| STM32H7 | ✓ (Hardware) | ✓ (Software) | Hardware JPEG + DMA2D |
| STM32F7 | ✓ (Hardware) | ✓ (Software) | Hardware JPEG |
| Other | ✗ | ✗ | None |

## Examples

### 1. CodecJpeg.ino
**JPEG image decoding and display**

- Loads and decodes JPEG images
- Displays decoded frames on LED matrix
- Handles different pixel formats (RGB565, RGB888)
- Includes scaling for different matrix sizes

**Hardware Requirements:**
- ESP32, Teensy 4.x, or STM32 microcontroller
- 64x64 LED matrix
- JPEG image data (replace sample data)

**Key Features:**
- Automatic platform detection
- Hardware acceleration when available
- Frame scaling and color conversion
- Error handling and fallback

### 2. CodecMpeg1.ino
**MPEG1 video playback on LED matrix**

- Streams and decodes MPEG1 video
- Real-time playback with configurable FPS
- Frame buffering for smooth playback
- Loop and single-frame modes

**Hardware Requirements:**
- ESP32, Teensy 4.x, or STM32 microcontroller
- 32x32 LED matrix (smaller for better performance)
- MPEG1 video data (replace sample data)

**Key Features:**
- Streaming video playback
- Configurable frame rate
- Memory-efficient buffering
- End-of-stream handling

## Basic Usage Pattern

### JPEG Decoding
```cpp
#include "fl/codec/jpeg.h"
#include "fl/bytestreammemory.h"

// Configure decoder
fl::JpegDecoderConfig config;
config.format = fl::PixelFormat::RGB888;
config.quality = fl::JpegDecoderConfig::Medium;

// Create decoder
auto decoder = fl::Jpeg::createDecoder(config);

// Create stream from data
auto stream = fl::make_shared<fl::ByteStreamMemory>(jpegData, dataSize);

// Decode
if (decoder->begin(stream)) {
    if (decoder->decode() == fl::DecodeResult::Success) {
        fl::DecodedVideoFrame frame = decoder->getCurrentFrame();
        // Use frame data...
    }
    decoder->end();
}
```

### MPEG1 Video Playback
```cpp
#include "fl/codec/mpeg1.h"

// Configure for real-time streaming (immediate mode)
fl::Mpeg1Config config;
config.mode = fl::Mpeg1Config::Streaming;
config.targetFps = 25;
config.immediateMode = true;  // Default: bypasses frame buffering for minimal latency

// For buffered mode (when you need frame buffering):
// config.immediateMode = false;
// config.bufferFrames = 2;

// Create decoder
auto decoder = fl::mpeg1::createDecoder(config);

// Playback loop
while (decoder->hasMoreFrames()) {
    if (decoder->decode() == fl::DecodeResult::Success) {
        fl::DecodedVideoFrame frame = decoder->getCurrentFrame();
        displayFrame(frame);
    }
}
```

## Performance Modes

### Immediate Mode (Default - Recommended for Real-Time)
- **Enabled by default**: `config.immediateMode = true`
- **Benefits**:
  - Minimal latency - frames available immediately after decode
  - Lower memory usage - no frame buffer vector allocation
  - Simpler code path - direct frame access
- **Best for**: Real-time LED applications, live video streaming

### Buffered Mode (Legacy)
- **Enable with**: `config.immediateMode = false`
- **Benefits**:
  - Multiple frame lookahead capability
  - Smoother playback for high-frame-rate content
- **Drawbacks**:
  - Adds frame buffering latency
  - Higher memory usage
- **Best for**: Non-real-time applications where smooth playback matters more than latency

## Memory Considerations

### Frame Buffer Sizes
- **RGB565**: 2 bytes per pixel
- **RGB888**: 3 bytes per pixel
- **RGBA8888**: 4 bytes per pixel

### Example Memory Usage
- 64x64 RGB888 frame: 12KB
- 32x32 RGB565 frame: 2KB
- MPEG1 streaming (2 buffers): 2x frame size

### Optimization Tips
1. Use RGB565 format for memory-constrained devices
2. Reduce matrix resolution for video playback
3. Limit buffer frames in streaming mode
4. Enable hardware acceleration when available

## Error Handling

All decoders provide comprehensive error reporting:

```cpp
fl::string error_msg;
auto decoder = fl::Jpeg::createDecoder(config, &error_msg);

if (!decoder) {
    Serial.println("Failed to create decoder: " + error_msg);
    return;
}

if (decoder->hasError(&error_msg)) {
    Serial.println("Decoder error: " + error_msg);
}
```

## Performance Tips

1. **Choose appropriate resolution**: Lower resolutions decode faster
2. **Use hardware acceleration**: Enable `useHardwareAcceleration = true`
3. **Optimize pixel format**: RGB565 is faster than RGB888
4. **Buffer management**: Use minimal buffer frames for streaming
5. **Platform-specific**: ESP32-S3 PIE optimizations are automatic

## Troubleshooting

### Common Issues

**"Codec not supported on this platform"**
- Check platform compatibility table
- Verify correct board selection in Arduino IDE

**Memory allocation failures**
- Reduce frame resolution
- Use RGB565 instead of RGB888
- Decrease buffer frame count

**Poor video performance**
- Lower target FPS
- Reduce LED matrix size
- Check for memory fragmentation

**JPEG decode failures**
- Verify JPEG data integrity
- Check supported JPEG format (baseline only)
- Ensure adequate memory

### Debug Output

Enable serial output for debugging:
```cpp
Serial.begin(115200);
Serial.println("Decoder status: " + (decoder->isReady() ? "Ready" : "Not Ready"));
```

## Building and Installation

1. Install FastLED library with codec support
2. Copy example to Arduino IDE
3. Replace sample data with actual JPEG/MPEG1 files
4. Adjust LED configuration for your hardware
5. Upload and test

## Further Reading

- [FastLED Codec Architecture](../../../.plans/mpeg.md)
- [Platform-Specific Optimizations](../../../src/platforms/)
- [Common Types and Utilities](../../../src/fl/codec/common.h)
- [Testing Documentation](../../../tests/unit/codec/)