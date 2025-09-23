# SimpleWebP

From: https://github.com/MikuAuahDark/simplewebp/commit/ea726e5aa110bac276fbcc5e90e7bd5456fc3ec1

On 2025-09-22



A simple, lightweight WebP decoder library for embedded systems and resource-constrained environments.

## Overview

SimpleWebP is a C library that provides WebP image decoding functionality with minimal dependencies. It supports both lossy and lossless WebP formats and includes features for custom memory allocation and input stream abstraction.

**Version**: 20231226
**Authors**: Google Inc., Miku AuahDark
**License**: See license at the bottom of `simplewebp.h`

## Features

- **Lossy and Lossless WebP Support**: Decode both VP8 (lossy) and VP8L (lossless) WebP images
- **Custom Memory Allocation**: Use your own allocator functions for memory management
- **Input Stream Abstraction**: Support for custom input sources (memory, files, etc.)
- **RGBA8 Output**: Decode to standard 32-bit RGBA pixel format
- **YUVA420 Planar Output**: Direct YUV output for lossy images (advanced usage)
- **Minimal Dependencies**: Single header file with no external dependencies

## Basic Usage

### Simple Memory-Based Decoding

```c
#include "simplewebp.h"
#include <stdio.h>
#include <stdlib.h>

// Load WebP from memory buffer
void decode_webp_from_memory(const uint8_t* webp_data, size_t data_size) {
    simplewebp* decoder = NULL;
    simplewebp_error error;

    // Load the WebP image
    error = simplewebp_load_from_memory((void*)webp_data, data_size, NULL, &decoder);
    if (error != SIMPLEWEBP_NO_ERROR) {
        printf("Error loading WebP: %s\n", simplewebp_get_error_text(error));
        return;
    }

    // Get image dimensions
    size_t width, height;
    simplewebp_get_dimensions(decoder, &width, &height);
    printf("Image size: %zux%zu\n", width, height);

    // Allocate buffer for RGBA pixels
    uint8_t* rgba_buffer = malloc(width * height * 4);
    if (!rgba_buffer) {
        simplewebp_unload(decoder);
        return;
    }

    // Decode to RGBA
    error = simplewebp_decode(decoder, rgba_buffer, NULL);
    if (error != SIMPLEWEBP_NO_ERROR) {
        printf("Error decoding WebP: %s\n", simplewebp_get_error_text(error));
        free(rgba_buffer);
        simplewebp_unload(decoder);
        return;
    }

    // Use the RGBA data...
    // rgba_buffer now contains width*height*4 bytes of RGBA data

    // Clean up
    free(rgba_buffer);
    simplewebp_unload(decoder);
}
```

### Custom Memory Allocator

```c
// Custom allocator example
static void* my_alloc(size_t size) {
    return malloc(size);  // Your custom allocation logic
}

static void my_free(void* ptr) {
    free(ptr);  // Your custom deallocation logic
}

void decode_with_custom_allocator(const uint8_t* webp_data, size_t data_size) {
    simplewebp_allocator allocator = {
        .alloc = my_alloc,
        .free = my_free
    };

    simplewebp* decoder = NULL;
    simplewebp_error error = simplewebp_load_from_memory(
        (void*)webp_data, data_size, &allocator, &decoder);

    // ... rest of decoding logic
}
```

## API Reference

### Core Types

- `simplewebp`: Opaque handle representing a loaded WebP image
- `simplewebp_error`: Error codes returned by API functions
- `simplewebp_allocator`: Custom memory allocator interface
- `simplewebp_input`: Input stream abstraction

### Main Functions

#### Loading Images
- `simplewebp_load_from_memory()`: Load WebP from memory buffer
- `simplewebp_load()`: Load WebP from custom input stream
- `simplewebp_unload()`: Free decoder and associated memory

#### Image Information
- `simplewebp_get_dimensions()`: Get image width and height
- `simplewebp_is_lossless()`: Check if image is lossless (VP8L) or lossy (VP8)

#### Decoding
- `simplewebp_decode()`: Decode to RGBA8 format
- `simplewebp_decode_yuva420()`: Decode to planar YUVA420 (lossy only)

#### Utilities
- `simplewebp_version()`: Get library version
- `simplewebp_get_error_text()`: Get human-readable error message

### Error Codes

- `SIMPLEWEBP_NO_ERROR`: Success
- `SIMPLEWEBP_ALLOC_ERROR`: Memory allocation failed
- `SIMPLEWEBP_IO_ERROR`: Input stream read error
- `SIMPLEWEBP_NOT_WEBP_ERROR`: Input is not a valid WebP image
- `SIMPLEWEBP_CORRUPT_ERROR`: WebP image data is corrupted
- `SIMPLEWEBP_UNSUPPORTED_ERROR`: Unsupported WebP variant
- `SIMPLEWEBP_IS_LOSSLESS_ERROR`: Operation not supported on lossless images

## Memory Management

SimpleWebP uses a flexible memory allocation system:

1. **Default Allocator**: Pass `NULL` to use standard `malloc`/`free`
2. **Custom Allocator**: Provide `simplewebp_allocator` struct with your functions

The library will automatically free all allocated memory when `simplewebp_unload()` is called.

## Input Stream Abstraction

For advanced usage, you can implement custom input sources by providing a `simplewebp_input` structure with read, seek, and tell callbacks. Use `simplewebp_input_from_memory()` for simple memory-based inputs.

## Limitations

- Only supports WebP images (VP8 and VP8L formats)
- YUVA420 planar decoding only works with lossy WebP images
- No encoding support (decode-only)
- No animation support for multi-frame WebP images

## Integration with FastLED

This library is used by FastLED's WebP codec (`fl/codec/webp.h`) to provide WebP decoding functionality. The FastLED wrapper provides a more convenient C++ interface following FastLED's coding conventions.
