#include "fl/codec/mpeg1.h"
#include "fl/stl/utility.h"
#include "fl/bytestreammemory.h"

namespace fl {

// MPEG1 factory implementation
IDecoderPtr Mpeg1::createDecoder(const Mpeg1Config& config, fl::string* error_message) {
    FL_UNUSED(error_message);
    // MPEG1 is currently supported via software decoder on all platforms
    return fl::make_shared<third_party::SoftwareMpeg1Decoder>(config);
}

bool Mpeg1::isSupported() {
    // Software MPEG1 decoder is available on all platforms
    return true;
}

Mpeg1Info Mpeg1::parseMpeg1Info(fl::span<const fl::u8> data, fl::string* error_message) {
    Mpeg1Info info;

    // Validate input data
    if (data.empty()) {
        if (error_message) {
            *error_message = "Empty MPEG1 data";
        }
        return info; // returns invalid info
    }

    // Minimum size check - MPEG1 needs at least the pack header and system header
    if (data.size() < 12) {
        if (error_message) {
            *error_message = "MPEG1 data too small";
        }
        return info;
    }

    // Check for MPEG1 start codes
    // Look for Pack Start Code (0x000001BA) or System Start Code (0x000001BB)
    bool foundMpegHeader = false;
    fl::size headerOffset = 0;

    for (fl::size i = 0; i <= data.size() - 4; i++) {
        if (data[i] == 0x00 && data[i + 1] == 0x00 && data[i + 2] == 0x01) {
            if (data[i + 3] == 0xBA || data[i + 3] == 0xBB) {
                foundMpegHeader = true;
                headerOffset = i;
                break;
            }
        }
    }

    if (!foundMpegHeader) {
        if (error_message) {
            *error_message = "Invalid MPEG1 stream - no valid start code found";
        }
        return info;
    }

    // For detailed parsing, we need to use the actual decoder to extract metadata
    // Since MPEG1 headers are complex, we'll use a temporary decoder instance
    Mpeg1Config tempConfig;
    tempConfig.mode = Mpeg1Config::SingleFrame;
    tempConfig.skipAudio = true;

    auto decoder = fl::make_shared<third_party::SoftwareMpeg1Decoder>(tempConfig);
    auto stream = fl::make_shared<fl::ByteStreamMemory>(data.size());
    stream->write(data.data(), data.size());

    if (decoder->begin(stream)) {
        // Extract metadata using decoder methods
        info.width = decoder->getWidth();
        info.height = decoder->getHeight();
        info.frameRate = decoder->getFrameRate();
        info.frameCount = decoder->getFrameCount();

        // Calculate duration if we have frame count and frame rate
        if (info.frameCount > 0 && info.frameRate > 0) {
            info.duration = (info.frameCount * 1000) / info.frameRate;
        }

        // MPEG1 can have audio, but our decoder is configured to skip it
        // In a real implementation, we'd parse the stream to detect audio tracks
        info.hasAudio = false;
        info.isValid = true;

        decoder->end();
    } else {
        // Fallback: basic MPEG1 sequence header parsing
        // Look for sequence header start code (0x000001B3)
        for (fl::size i = headerOffset; i <= data.size() - 8; i++) {
            if (data[i] == 0x00 && data[i + 1] == 0x00 &&
                data[i + 2] == 0x01 && data[i + 3] == 0xB3) {

                // Found sequence header, parse basic info
                if (i + 7 < data.size()) {
                    // Width is in bits 4-15 of bytes 4-5
                    // Height is in bits 4-15 of bytes 6-7
                    fl::u16 width = ((data[i + 4] << 4) | ((data[i + 5] & 0xF0) >> 4));
                    fl::u16 height = (((data[i + 5] & 0x0F) << 8) | data[i + 6]);

                    if (width > 0 && height > 0) {
                        info.width = width;
                        info.height = height;
                        info.frameRate = 25; // Default assumption
                        info.frameCount = 0; // Unknown
                        info.duration = 0;   // Unknown
                        info.hasAudio = false;
                        info.isValid = true;
                    }
                }
                break;
            }
        }

        if (!info.isValid && error_message) {
            *error_message = "Failed to parse MPEG1 stream metadata";
        }
    }

    return info;
}

} // namespace fl
