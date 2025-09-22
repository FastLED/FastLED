# TJpg_Decoder Integration for FastLED

Pulled from: https://github.com/Bodmer/TJpg_Decoder/commit/71bfc2607b6963ee3334ff6f601345c5d2b7a8da

## Current Status
- ✅ **Library Staged**: Third-party code copied to FastLED
- ❌ **Namespace Wrapping**: Not yet wrapped in `fl::third_party`
- ❌ **C++ Conversion**: `tjpgd.c` not yet converted to `tjpgd.cpp`
- ❌ **FastLED Integration**: Bridge to codec architecture not implemented

## Implementation Plan

### Phase 1: Library Modernization
1. **C++ Conversion**
   - `tjpgd.c` → `tjpgd.cpp`
   - Update includes and linkage

2. **Namespace Wrapping**
   ```cpp
   namespace fl {
   namespace third_party {
       // All TJpg_Decoder code wrapped here
   }
   }
   ```

3. **Dependency Cleanup**
   - Remove Arduino-specific includes (`Arduino.h`, `FS.h`, etc.)
   - Replace `String` with `fl::string`
   - Remove filesystem dependencies for ByteStream compatibility

### Phase 2: FastLED Integration
1. **Bridge Implementation** (`src/fl/codec/jpeg_tjpg_decoder.h`)
   ```cpp
   class TJpgDecoder : public JpegDecoderBase {
       fl::third_party::TJpg_Decoder decoder_;
       // Bridge between FastLED codec API and TJpg_Decoder
   };
   ```

2. **Configuration Mapping**
   - `JpegConfig::Quality` → TJpg scaling factor
   - `JpegConfig::format` → pixel format conversion
   - `JpegConfig::maxWidth/Height` → size validation

3. **Memory Management**
   - Replace Arduino memory allocation
   - Use FastLED's `fl::scoped_array` and buffer management
   - Convert callback-based output to frame buffer approach

### Phase 3: Integration & Testing
1. **Factory Registration**
   - Update `fl::jpeg::createDecoder()` to return `TJpgDecoder`
   - Platform detection and capability reporting

2. **Error Handling**
   - Map `JRESULT` codes to FastLED error system
   - Integrate with `JpegDecoderBase::setError()`

3. **Testing**
   - Unit tests for decoder functionality
   - Integration tests with codec architecture
   - Memory usage validation

## Key Challenges
- **Callback Architecture**: Transform coordinate-based drawing to frame buffer
- **Memory Model**: Align Arduino patterns with FastLED memory management
- **Platform Dependencies**: Remove ESP32/Arduino-specific filesystem code
- **Performance**: Maintain efficient decoding while adding abstraction layer

## Unit Test Design

Existing test framework: `tests/codec_jpeg.cpp`

### Current Test Coverage
- ✅ **Availability Testing**: `jpeg::isSupported()` detection
- ✅ **Factory Testing**: `jpeg::createDecoder()` with error handling
- ✅ **Lifecycle Testing**: `begin()`, `end()`, and state management
- ✅ **Configuration Testing**: `JpegConfig` parameter validation
- ✅ **Error Handling**: Null stream and invalid input handling

### Additional Tests Required for TJpg Integration

#### Phase 1: Basic Functionality
```cpp
TEST_CASE("JPEG TJpg decoder initialization") {
    // Test TJpgDecoder specific initialization
    // Verify workspace allocation
    // Check namespace wrapping
}

TEST_CASE("JPEG valid file decoding") {
    // Test with minimal valid JPEG data
    // Verify frame dimensions and pixel data
    // Check memory allocation patterns
}
```

#### Phase 2: Format Support
```cpp
TEST_CASE("JPEG pixel format conversion") {
    // Test RGB888, RGB565, RGBA8888 output formats
    // Verify color space conversion accuracy
    // Check byte ordering and alignment
}

TEST_CASE("JPEG scaling and quality") {
    // Test JpegConfig::Quality mapping to TJpg scale factors
    // Verify image scaling (1/1, 1/2, 1/4, 1/8)
    // Performance benchmarks for different settings
}
```

#### Phase 3: Edge Cases & Performance
```cpp
TEST_CASE("JPEG large image handling") {
    // Test maxWidth/maxHeight enforcement
    // Memory usage validation
    // Progressive JPEG support
}

TEST_CASE("JPEG malformed data") {
    // Truncated files, invalid headers
    // Error recovery and cleanup
    // Memory leak detection
}

TEST_CASE("JPEG stress testing") {
    // Multiple decode operations
    // Concurrent decoder instances
    // Memory fragmentation scenarios
}
```

### Test Data Requirements
- **Minimal JPEG**: 1x1 pixel valid JPEG for basic tests
- **Format Samples**: RGB/Grayscale/Progressive variants
- **Size Variants**: Small (64x64) to large (1920x1080) images
- **Malformed Data**: Corrupted headers, truncated streams

### Integration with Existing Test Suite
Update `tests/codec_jpeg.cpp` to:
1. Replace `CHECK_FALSE(jpegSupported)` with `CHECK(jpegSupported)` post-implementation
2. Add TJpg-specific test cases alongside generic codec tests
3. Ensure tests pass both with and without TJpg_Decoder available

## API Integration
Main FastLED API entry point: `src/fl/codec/jpeg.h`
- `fl::jpeg::createDecoder()` - Factory function
- `JpegDecoderBase` - Base class interface
- `JpegConfig` - Configuration structure