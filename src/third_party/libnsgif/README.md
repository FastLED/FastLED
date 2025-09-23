# Libnsgif Integration for FastLED

Pulled from: https://www.netsurf-browser.org/projects/libnsgif/

## Current Status
- ✅ **Library Staged**: Third-party code copied to FastLED
- ✅ **IDecoder Interface**: Created `fl/codec/idecoder.h` with base decoder interface
- ⏳ **Namespace Wrapping**: Needs wrapping in `fl::third_party`
- ⏳ **C++ Conversion**: `.c` files need conversion to `.cpp`
- ⏳ **FastLED Integration**: Bridge implementation needed in `fl/codec/gif.cpp`
- ⏳ **Decoder Implementation**: `fl::third_party::SoftwareGifDecoder` class in `software_decoder.h`
- ⏳ **Factory API**: `Gif::createDecoder()` function to return `shared_ptr<IDecoder>`
- ⏳ **Memory Management**: Integration with FastLED's buffer management and scoped arrays
- ⏳ **Pixel Format Support**: RGB888, RGB565, RGBA8888 conversion implementation needed
- ⏳ **Configuration Structure**: `GifConfig` integration required
- ⏳ **Animation Support**: Multi-frame streaming via IDecoder pattern

## FastLED Third-Party Namespace Architecture

FastLED mandates that all third-party libraries be wrapped in the `fl::third_party` namespace to:

### 1. **Prevent Global Namespace Pollution**
```cpp
// Before: Global namespace collision risk
struct nsgif { /* ... */ };

// After: Safely namespaced
namespace fl {
namespace third_party {
    struct nsgif { /* ... */ };
}
}
```

### 2. **Clear Ownership and Licensing Boundaries**
The `fl::third_party` namespace makes it immediately clear:
- **What code is external**: Everything in `fl::third_party` comes from external sources
- **Licensing considerations**: Third-party code may have different licenses
- **Maintenance responsibility**: External libraries have their own update cycles
- **API stability**: Third-party APIs may change independently of FastLED

### 3. **Controlled Integration Points**
```cpp
// Third-party code stays isolated
namespace fl::third_party {
    struct nsgif { /* Original C API */ };
    nsgif_error nsgif_create(nsgif **gif, nsgif_bitmap_cb_vt *bitmap_callbacks);

    // Software decoder implementation within third-party namespace
    class SoftwareGifDecoder : public fl::IDecoder {
        nsgif* decoder_;  // Direct access to nsgif within same namespace
        // IDecoder interface implementation for animated GIFs
    };
}

// FastLED provides clean factory interface
namespace fl {
    // GIF decoder factory (following MPEG1 pattern for multi-frame formats)
    class Gif {
    public:
        // Create a GIF decoder for the current platform
        static fl::shared_ptr<IDecoder> createDecoder(const GifConfig& config, fl::string* error_message = nullptr);

        // Check if GIF decoding is supported on this platform
        static bool isSupported();
    };
}
```

### 4. **Implementation Benefits**

#### **Namespace Isolation**
- No symbol conflicts with user code or other libraries
- Clear separation between FastLED native code and external dependencies
- Enables multiple versions of similar libraries if needed

#### **API Transformation**
- Third-party libraries keep their original APIs intact
- FastLED provides idiomatic C++ wrappers that follow project conventions
- Users interact with clean FastLED APIs, not raw third-party interfaces

#### **Maintenance Advantages**
- Updates to third-party libraries are contained within their namespace
- FastLED can provide compatibility layers when third-party APIs change
- Clear responsibility boundaries for bug fixes and feature development

### 5. **Libnsgif Example**

The libnsgif implementation will demonstrate this pattern:

```cpp
// File: nsgif.h
namespace fl {
namespace third_party {
    // Original libnsgif API preserved
    typedef struct nsgif nsgif;
    typedef enum {
        NSGIF_OK,
        NSGIF_INSUFFICIENT_MEMORY,
        NSGIF_DATA_ERROR,
        // ... other error codes
    } nsgif_error;

    nsgif_error nsgif_create(nsgif **gif, nsgif_bitmap_cb_vt *bitmap_callbacks);
    nsgif_error nsgif_data_scan(nsgif *gif, size_t size, const uint8_t *data);
    nsgif_error nsgif_frame_decode(nsgif *gif, uint32_t frame, nsgif_bitmap_t **bitmap);
}
}

// File: fl/codec/gif.h
namespace fl {
    // GIF-specific configuration
    struct GifConfig {
        enum FrameMode { SingleFrame, Streaming };

        FrameMode mode = Streaming;
        PixelFormat format = PixelFormat::RGB888;
        bool looping = true;
        fl::u16 maxWidth = 1920;
        fl::u16 maxHeight = 1080;
        fl::u8 bufferFrames = 3;  // For smooth animation
    };

    // GIF decoder factory
    class Gif {
    public:
        // Create a GIF decoder for the current platform
        static fl::shared_ptr<IDecoder> createDecoder(const GifConfig& config, fl::string* error_message = nullptr);

        // Check if GIF decoding is supported on this platform
        static bool isSupported();
    };

    // Software GIF decoder implementation (in fl::third_party namespace)
    namespace third_party {
        class SoftwareGifDecoder : public IDecoder {
            nsgif* decoder_;  // Direct access to nsgif within same namespace
            // Complete IDecoder interface implementation for animated GIFs
        };
    }
}
```

### 6. **Guidelines for Third-Party Integration**

When adding new third-party libraries to FastLED:

1. **Always use `fl::third_party` namespace** - Non-negotiable requirement
2. **Preserve original APIs** - Don't modify third-party code unnecessarily
3. **Create FastLED wrappers** - Provide clean C++ APIs following project conventions
4. **Document boundaries clearly** - Make it obvious what's third-party vs. FastLED native
5. **Handle type conversions** - Bridge between third-party types and FastLED types
6. **Manage memory correctly** - Use FastLED memory management patterns in wrappers

This architecture ensures FastLED remains maintainable, extensible, and free from the complexities often introduced by direct third-party library integration.

## Implementation Plan

### 🔄 Phase 1: Library Modernization (PENDING)
1. **C++ Conversion**
   - ⏳ `gif.c` → `gif.cpp`
   - ⏳ `lzw.c` → `lzw.cpp`
   - ⏳ Updated includes and linkage

2. **Namespace Wrapping**
   ```cpp
   namespace fl {
   namespace third_party {
       // All libnsgif code wrapped here
   }
   }
   ```
   - ⏳ All libnsgif structures and functions properly namespaced

3. **Dependency Cleanup**
   - ⏳ Platform-specific includes cleaned up
   - ⏳ Integration with FastLED type system
   - ⏳ ByteStream compatibility implemented

### 🔄 Phase 2: FastLED Integration (PENDING)
1. **Bridge Implementation** (`src/third_party/libnsgif/software_decoder.h`)
   ```cpp
   namespace fl::third_party {
       class SoftwareGifDecoder : public fl::IDecoder {
           nsgif* decoder_;
           fl::GifConfig config_;
           // Complete IDecoder interface implementation
           bool begin(fl::ByteStreamPtr stream) override;
           DecodeResult decode() override;
           Frame getCurrentFrame() override;
           bool hasMoreFrames() const override;
           fl::u32 getFrameCount() const override;
           bool seek(fl::u32 frameIndex) override;
       };
   }
   ```
   - ⏳ Full `IDecoder` interface implementation in third-party namespace

2. **Factory Class Implementation** (`src/fl/codec/gif.cpp`)
   ```cpp
   namespace fl {
       fl::shared_ptr<IDecoder> Gif::createDecoder(const GifConfig& config, fl::string* error_message) {
           // Create and return fl::third_party::SoftwareGifDecoder instance
           return fl::make_shared<fl::third_party::SoftwareGifDecoder>(config);
       }

       bool Gif::isSupported() {
           return true; // libnsgif is always available
       }
   }
   ```
   - ⏳ Factory class following MPEG1 pattern for multi-frame formats

3. **Configuration Mapping**
   - ⏳ `GifConfig::looping` → GIF animation loop handling
   - ⏳ `GifConfig::format` → pixel format conversion
   - ⏳ `GifConfig::maxWidth/Height` → size validation
   - ⏳ `GifConfig::mode` → SingleFrame vs Streaming animation
   - ⏳ `GifConfig::bufferFrames` → animation frame buffering

4. **Memory Management**
   - ⏳ C memory allocation replaced with FastLED patterns
   - ⏳ `fl::scoped_array` and buffer management implemented
   - ⏳ Callback-based bitmap creation converted to frame buffer approach

### 🔄 Phase 3: Integration & Testing (PENDING)
1. **Factory Class Implementation**
   - ⏳ `Gif::createDecoder()` factory function implemented
   - ⏳ `Gif::isSupported()` platform detection function
   - ⏳ Clean public API in `fl/codec/gif.h`
   - ⏳ Integration with existing codec framework

2. **IDecoder Interface Implementation**
   - ⏳ `begin()` - Initialize with ByteStream for streaming GIF data
   - ⏳ `decode()` - Decode next frame with proper state management
   - ⏳ `getCurrentFrame()` - Return current decoded frame
   - ⏳ `hasMoreFrames()` - Check for additional animation frames
   - ⏳ `getFrameCount()` - Total frame count for animations
   - ⏳ `seek()` - Jump to specific frame index

3. **Error Handling**
   - ⏳ `nsgif_error` codes mapped to FastLED `DecodeResult` enum
   - ⏳ Comprehensive error message reporting via `hasError()`
   - ⏳ Graceful handling of malformed GIF data

4. **Advanced Features**
   - ⏳ Multiple pixel format support (RGB888, RGB565, RGBA8888)
   - ⏳ Animation frame iteration and timing
   - ⏳ Transparency and disposal method handling
   - ⏳ Memory-efficient streaming decode with frame buffering

## Key Challenges (TO BE ADDRESSED)
- ⏳ **Callback Architecture**: Transform bitmap callback system to frame buffer approach
- ⏳ **Memory Model**: C patterns need replacement with FastLED's `fl::scoped_array` and proper buffer management
- ⏳ **Animation Support**: GIF animation timing and frame sequencing integration using IDecoder pattern
- ⏳ **Transparency Handling**: Alpha channel support and background disposal methods
- ⏳ **Performance**: Efficient decoding with minimal memory allocation
- ⏳ **Streaming Support**: ByteStream integration for progressive GIF loading

## Planned API

The GIF decoder will provide comprehensive animated GIF support using the IDecoder interface:

### Basic Usage

```cpp
#include "fl/codec/gif.h"

// Create GIF decoder for animated content
fl::GifConfig config;
config.format = fl::PixelFormat::RGB888;
config.mode = fl::GifConfig::Streaming;  // For animations
config.looping = true;

fl::string error;
auto decoder = fl::Gif::createDecoder(config, &error);
if (!decoder) {
    printf("GIF decoder creation failed: %s\n", error.c_str());
    return;
}

// Initialize with GIF data stream
auto stream = fl::ByteStream::fromSpan(gif_data_span);
if (!decoder->begin(stream)) {
    printf("Failed to initialize GIF decoder\n");
    return;
}
```

### Animation Support

```cpp
// Decode animation frames
while (decoder->hasMoreFrames()) {
    auto result = decoder->decode();
    if (result == fl::DecodeResult::Success) {
        fl::Frame frame = decoder->getCurrentFrame();
        // Display or process the frame
        displayFrame(frame);

        // Get current frame info
        fl::u32 currentIndex = decoder->getCurrentFrameIndex();
        printf("Displaying frame %u of %u\n", currentIndex, decoder->getFrameCount());
    } else if (result == fl::DecodeResult::EndOfStream) {
        // Animation complete
        break;
    } else if (result == fl::DecodeResult::Error) {
        fl::string error;
        decoder->hasError(&error);
        printf("Decode error: %s\n", error.c_str());
        break;
    }
}

// Cleanup
decoder->end();
```

### Random Access Support

```cpp
// Jump to specific frame
fl::u32 targetFrame = 10;
if (decoder->seek(targetFrame)) {
    auto result = decoder->decode();
    if (result == fl::DecodeResult::Success) {
        fl::Frame frame = decoder->getCurrentFrame();
        // Process frame 10
    }
}
```

### Advanced Configuration

```cpp
// Custom configuration for memory-constrained environments
fl::GifConfig config;
config.format = fl::PixelFormat::RGB565;        // 16-bit color to save memory
config.maxWidth = 800;                          // Size limits
config.maxHeight = 600;
config.bufferFrames = 2;                        // Minimal buffering
config.mode = fl::GifConfig::SingleFrame;       // Just decode one frame
```

### Platform Support

```cpp
// Check if GIF decoding is available
if (fl::Gif::isSupported()) {
    // GIF decoding available
} else {
    // Platform doesn't support GIF
}
```

### Integration with FastLED Codecs

The GIF decoder integrates seamlessly with FastLED's codec architecture:
- Implements `IDecoder` interface for consistent API
- Uses `fl::PixelFormat` for output format specification
- Compatible with `Frame` and `FramePtr` types
- Follows FastLED error handling conventions with `DecodeResult`
- Works with `fl::ByteStreamPtr` for streaming input data
- Supports animation timing and frame sequencing through `hasMoreFrames()` and `seek()`

## Unit Test Design

Planned test framework: `tests/codec_gif.cpp`

### Required Test Coverage

#### Phase 1: Basic Functionality
```cpp
TEST_CASE("GIF libnsgif decoder initialization") {
    // Test NsGifDecoder specific initialization
    // Verify callback setup and memory allocation
    // Check namespace wrapping
}

TEST_CASE("GIF valid file decoding") {
    // Test with minimal valid GIF data
    // Verify frame dimensions and pixel data
    // Check memory allocation patterns
}
```

#### Phase 2: Animation Support
```cpp
TEST_CASE("GIF animation frame handling") {
    // Test multi-frame GIF decoding
    // Verify frame count detection
    // Check frame-by-frame decoding accuracy
}

TEST_CASE("GIF transparency and disposal") {
    // Test transparent GIF handling
    // Verify background disposal methods
    // Check alpha channel preservation
}
```

#### Phase 3: Format Support
```cpp
TEST_CASE("GIF pixel format conversion") {
    // Test RGB888, RGB565, RGBA8888 output formats
    // Verify color palette conversion accuracy
    // Check byte ordering and alignment
}

TEST_CASE("GIF interlaced decoding") {
    // Test interlaced GIF support
    // Verify progressive rendering capability
    // Performance benchmarks for different settings
}
```

#### Phase 4: Edge Cases & Performance
```cpp
TEST_CASE("GIF large animation handling") {
    // Test maxWidth/maxHeight enforcement
    // Memory usage validation with many frames
    // Animation timing accuracy
}

TEST_CASE("GIF malformed data") {
    // Truncated files, invalid headers
    // Corrupted frame data recovery
    // Memory leak detection
}

TEST_CASE("GIF stress testing") {
    // Multiple decode operations
    // Concurrent decoder instances
    // Large animation sequences
}
```

### Test Data Requirements
- **Minimal GIF**: 1x1 pixel valid GIF for basic tests
- **Animation Samples**: Simple 2-3 frame animations
- **Transparency Tests**: GIFs with transparent backgrounds
- **Interlaced GIFs**: Progressive rendering test cases
- **Size Variants**: Small (64x64) to large (800x600) animations
- **Malformed Data**: Corrupted headers, truncated streams

### Integration with Existing Test Suite
⏳ **Status**: Test suite to be created
1. ⏳ Create `tests/codec_gif.cpp` following existing codec test patterns
2. ⏳ Add libnsgif-specific test cases alongside generic codec tests
3. ⏳ Integration with FastLED test runner

## API Integration (PLANNED)

### File Structure
- **`src/fl/codec/idecoder.h`** ✅ - Base `IDecoder` interface and `DecodeResult` enum
- **`src/fl/codec/gif.h`** ⏳ - Public API entry point with factory functions
- **`src/fl/codec/gif.cpp`** ⏳ - Factory implementation bridging to third-party decoder
- **`src/third_party/libnsgif/software_decoder.h`** ⏳ - `SoftwareGifDecoder` implementation
- **`src/third_party/libnsgif/software_decoder.cpp`** ⏳ - Decoder implementation details

### API Components
- ⏳ `fl::Gif::createDecoder()` - Factory function returning `shared_ptr<IDecoder>`
- ⏳ `fl::Gif::isSupported()` - Platform detection function
- ⏳ `fl::GifConfig` - Configuration structure for animated GIFs
- ⏳ `fl::third_party::SoftwareGifDecoder` - IDecoder implementation class in third-party namespace
- ⏳ Full integration with FastLED `Frame`, `PixelFormat`, and `ByteStream` systems
- ⏳ Complete `IDecoder` interface support for animation streaming

## Libnsgif Library Overview

Libnsgif is a decoding library for GIF image format, originally developed for the NetSurf web browser. Key features include:

### Core Capabilities
- **Complete GIF87a and GIF89a support** - Handles all standard GIF variants
- **Animation support** - Full multi-frame GIF animation decoding
- **Transparency handling** - Proper alpha channel and background disposal
- **Interlaced support** - Progressive GIF rendering capability
- **Memory efficient** - Streaming decode with minimal memory footprint
- **C API** - Clean, well-documented C interface

### Technical Specifications
- **LZW decompression** - Optimized Lempel-Ziv-Welch implementation
- **Color palette** - Up to 256 colors with transparency support
- **Frame management** - Individual frame extraction and timing information
- **Error handling** - Comprehensive error reporting and recovery
- **Platform independent** - Pure C implementation with no external dependencies

### Integration Advantages
- **Proven stability** - Used in production NetSurf browser
- **Comprehensive format support** - Handles edge cases and malformed files gracefully
- **Small footprint** - Minimal code size and memory requirements
- **Clean API** - Well-designed interface suitable for wrapping
- **No dependencies** - Self-contained implementation

## Summary

The libnsgif integration is **PLANNED** and represents the next step in FastLED's codec expansion. Once complete, the GIF decoder will provide:

1. **Full GIF format support** including animations and transparency
2. **Clean C++ API** following FastLED conventions
3. **Multiple pixel formats** (RGB888, RGB565, RGBA8888)
4. **Animation frame control** with timing and sequencing
5. **Memory-efficient operation** using FastLED buffer management
6. **Comprehensive error handling** with descriptive messages
7. **Full test coverage** and validation

The implementation will follow the established third-party integration pattern used successfully with TJpg_Decoder, ensuring consistent architecture and maintainability across FastLED's codec ecosystem.