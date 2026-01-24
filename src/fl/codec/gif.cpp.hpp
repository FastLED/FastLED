#include "gif.h"
#include "third_party/libnsgif/software_decoder.h"
#include "fl/bytestreammemory.h"

namespace fl {

IDecoderPtr Gif::createDecoder(const GifConfig& config, fl::string* error_message) {
    // Create the software GIF decoder
    auto decoder = fl::make_shared<fl::third_party::SoftwareGifDecoder>(config.format);

    if (!decoder) {
        if (error_message) {
            *error_message = "Failed to allocate GIF decoder";
        }
        return nullptr;
    }

    return decoder;
}

bool Gif::isSupported() {
    // libnsgif is always available since it's included directly
    return true;
}

GifInfo Gif::parseGifInfo(fl::span<const fl::u8> data, fl::string* error_message) {
    GifInfo info;

    // Validate input data
    if (data.empty()) {
        if (error_message) {
            *error_message = "Empty GIF data";
        }
        return info; // returns invalid info
    }

    // Minimum size check - GIF must have at least the header (6 bytes) + some content
    if (data.size() < 13) { // GIF header (6) + screen descriptor (7)
        if (error_message) {
            *error_message = "GIF data too small";
        }
        return info;
    }

    // Check GIF signature
    if (data[0] != 'G' || data[1] != 'I' || data[2] != 'F') {
        if (error_message) {
            *error_message = "Invalid GIF signature";
        }
        return info;
    }

    // Check version (87a or 89a)
    if (!((data[3] == '8' && data[4] == '7' && data[5] == 'a') ||
          (data[3] == '8' && data[4] == '9' && data[5] == 'a'))) {
        if (error_message) {
            *error_message = "Unsupported GIF version";
        }
        return info;
    }

    // Parse logical screen descriptor (starts at byte 6)
    // Width (2 bytes, little endian) at offset 6-7
    // Height (2 bytes, little endian) at offset 8-9
    fl::u16 width = data[6] | (data[7] << 8);
    fl::u16 height = data[8] | (data[9] << 8);

    if (width == 0 || height == 0) {
        if (error_message) {
            *error_message = "Invalid GIF dimensions";
        }
        return info;
    }

    // For a more complete parsing, we would need to create a temporary decoder
    // and let libnsgif parse the structure. Let's use the existing decoder briefly.
    auto decoder = fl::make_shared<fl::third_party::SoftwareGifDecoder>(PixelFormat::RGB888);
    auto stream = fl::make_shared<fl::ByteStreamMemory>(data.size());
    stream->write(data.data(), data.size());

    if (decoder->begin(stream)) {
        // Now we can extract more detailed info using the decoder's methods
        info.width = decoder->getWidth();
        info.height = decoder->getHeight();
        info.frameCount = decoder->getFrameCount();
        info.loopCount = decoder->getLoopCount();
        info.isAnimated = decoder->isAnimated();
        info.bitsPerPixel = 8; // GIF is always 8 bits per pixel
        info.isValid = true;

        decoder->end();
    } else {
        // Fallback to basic header parsing if decoder fails
        info.width = width;
        info.height = height;
        info.frameCount = 1; // Assume static image
        info.loopCount = 0;
        info.isAnimated = false;
        info.bitsPerPixel = 8;
        info.isValid = true;

        if (error_message) {
            *error_message = ""; // Clear any error message for fallback case
        }
    }

    return info;
}

} // namespace fl
