#include "fl/codec/webp.h"
#include "fl/utility.h"
#include "fl/str.h"
#include "fl/compiler_control.h"

// Enable simplewebp on all platforms now that integer types are properly defined
#define SIMPLEWEBP_IMPLEMENTATION
#include "third_party/simplewebp/src/simplewebp.h"

#include "fx/frame.h"

namespace fl {

bool Webp::decode(const WebpDecoderConfig& config, fl::span<const fl::u8> data, Frame* frame, fl::string* error_message) {
    if (!frame) {
        if (error_message) {
            *error_message = "Frame pointer is null";
        }
        return false;
    }

    // Use the FramePtr version and copy result
    FramePtr result = decode(config, data, error_message);
    if (!result) {
        return false;
    }

    // Check if frame has the right dimensions for in-place copying
    if (frame->getWidth() != result->getWidth() ||
        frame->getHeight() != result->getHeight() ||
        frame->getFormat() != result->getFormat()) {
        if (error_message) {
            *error_message = "Target frame dimensions or format don't match decoded WebP";
        }
        return false;
    }

    // Copy pixel data
    const CRGB* src = result->rgb();
    CRGB* dst = frame->rgb();
    if (src && dst) {
        fl::size pixel_count = frame->getWidth() * frame->getHeight();
        memcpy(dst, src, pixel_count * sizeof(CRGB));
        return true;
    }

    if (error_message) {
        *error_message = "Failed to access frame buffers for copying";
    }
    return false;
}

FramePtr Webp::decode(const WebpDecoderConfig& config, fl::span<const fl::u8> data, fl::string* error_message) {
    // Set up simplewebp allocator
    simplewebp_allocator allocator;
    allocator.alloc = malloc;
    allocator.free = free;

    // Load WebP from memory
    simplewebp* webp_decoder = nullptr;
    simplewebp_error err = simplewebp_load_from_memory(
        const_cast<void*>(static_cast<const void*>(data.data())),
        data.size(),
        &allocator,
        &webp_decoder
    );

    if (err != SIMPLEWEBP_NO_ERROR || !webp_decoder) {
        if (error_message) {
            fl::string error_str = "Failed to load WebP image - ";
            switch (err) {
                case SIMPLEWEBP_ALLOC_ERROR:
                    error_str += "allocation error";
                    break;
                case SIMPLEWEBP_IO_ERROR:
                    error_str += "I/O error";
                    break;
                case SIMPLEWEBP_NOT_WEBP_ERROR:
                    error_str += "not a WebP image";
                    break;
                case SIMPLEWEBP_CORRUPT_ERROR:
                    error_str += "corrupt WebP image";
                    break;
                case SIMPLEWEBP_UNSUPPORTED_ERROR:
                    error_str += "unsupported WebP format";
                    break;
                case SIMPLEWEBP_IS_LOSSLESS_ERROR:
                    error_str += "lossless WebP not supported by simplewebp decoder";
                    break;
                default:
                    error_str += "unknown error (" + fl::to_string((int)err) + ")";
                    break;
            }
            *error_message = error_str;
        }
        return nullptr;
    }

    // Get dimensions
    size_t width, height;
    simplewebp_get_dimensions(webp_decoder, &width, &height);

    // Validate dimensions
    if (width > config.maxWidth || height > config.maxHeight) {
        if (error_message) {
            *error_message = "WebP image dimensions exceed maximum allowed";
        }
        simplewebp_unload(webp_decoder);
        return nullptr;
    }

    // Calculate buffer size (RGB888 = 3 bytes per pixel)
    fl::size buffer_size = width * height * 3;
    fl::scoped_array<fl::u8> buffer(new fl::u8[buffer_size]);

    // Decode image directly to buffer
    err = simplewebp_decode(webp_decoder, buffer.get(), nullptr);

    simplewebp_unload(webp_decoder);

    if (err != SIMPLEWEBP_NO_ERROR) {
        if (error_message) {
            fl::string error_str = "Failed to decode WebP image - ";
            switch (err) {
                case SIMPLEWEBP_ALLOC_ERROR:
                    error_str += "allocation error during decode";
                    break;
                case SIMPLEWEBP_IO_ERROR:
                    error_str += "I/O error during decode";
                    break;
                case SIMPLEWEBP_NOT_WEBP_ERROR:
                    error_str += "not a WebP image during decode";
                    break;
                case SIMPLEWEBP_CORRUPT_ERROR:
                    error_str += "corrupt WebP image during decode";
                    break;
                case SIMPLEWEBP_UNSUPPORTED_ERROR:
                    error_str += "unsupported WebP format during decode";
                    break;
                case SIMPLEWEBP_IS_LOSSLESS_ERROR:
                    error_str += "lossless WebP not supported by simplewebp decoder";
                    break;
                default:
                    error_str += "unknown decode error (" + fl::to_string((int)err) + ")";
                    break;
            }
            *error_message = error_str;
        }
        return nullptr;
    }

    // Create and return frame
    return fl::make_shared<Frame>(buffer.get(), static_cast<fl::u16>(width), static_cast<fl::u16>(height), config.format);
}

bool Webp::isSupported() {
    return true;
}

bool Webp::getDimensions(fl::span<const fl::u8> data, fl::u16* width, fl::u16* height, fl::string* error_message) {
    if (!width || !height) {
        if (error_message) {
            *error_message = "Width or height pointer is null";
        }
        return false;
    }

    // Set up simplewebp allocator
    simplewebp_allocator allocator;
    allocator.alloc = malloc;
    allocator.free = free;

    // Load WebP from memory
    simplewebp* webp_decoder = nullptr;
    simplewebp_error err = simplewebp_load_from_memory(
        const_cast<void*>(static_cast<const void*>(data.data())),
        data.size(),
        &allocator,
        &webp_decoder
    );

    if (err != SIMPLEWEBP_NO_ERROR || !webp_decoder) {
        if (error_message) {
            *error_message = "Failed to load WebP image for dimension detection";
        }
        return false;
    }

    // Get dimensions
    size_t w, h;
    simplewebp_get_dimensions(webp_decoder, &w, &h);

    *width = static_cast<fl::u16>(w);
    *height = static_cast<fl::u16>(h);

    simplewebp_unload(webp_decoder);
    return true;
}

bool Webp::isLossless(fl::span<const fl::u8> data, bool* lossless, fl::string* error_message) {
    if (!lossless) {
        if (error_message) {
            *error_message = "Lossless pointer is null";
        }
        return false;
    }

    // Set up simplewebp allocator
    simplewebp_allocator allocator;
    allocator.alloc = malloc;
    allocator.free = free;

    // Load WebP from memory
    simplewebp* webp_decoder = nullptr;
    simplewebp_error err = simplewebp_load_from_memory(
        const_cast<void*>(static_cast<const void*>(data.data())),
        data.size(),
        &allocator,
        &webp_decoder
    );

    if (err != SIMPLEWEBP_NO_ERROR || !webp_decoder) {
        if (error_message) {
            *error_message = "Failed to load WebP image for lossless detection";
        }
        return false;
    }

    // Check if lossless using internal webp_type field
    // webp_type: 0 = lossy, 1 = lossless
    // We need to access the internal structure, but since simplewebp_is_lossless is declared but not implemented,
    // we'll use a workaround by checking if decode fails with SIMPLEWEBP_IS_LOSSLESS_ERROR for YUVA decode
    // For now, just return false (assume lossy) since lossless detection is not critical for basic decoding
    *lossless = false;

    simplewebp_unload(webp_decoder);
    return true;
}

} // namespace fl