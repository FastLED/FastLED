#pragma once

#include "fl/codec/common.h"
#include "third_party/mpeg1_decoder/software_decoder.h"

namespace fl {

// Re-export MPEG1 configuration from third_party
using Mpeg1Config = third_party::Mpeg1Config;

// MPEG1 decoder factory
class Mpeg1 {
public:
    // Create an MPEG1 decoder for the current platform
    static IDecoderPtr createDecoder(const Mpeg1Config& config, fl::string* error_message = nullptr);

    // Check if MPEG1 decoding is supported on this platform
    static bool isSupported();
};

} // namespace fl
