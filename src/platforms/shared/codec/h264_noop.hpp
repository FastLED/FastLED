// IWYU pragma: private
#pragma once

#include "fl/codec/h264.h"
#include "fl/codec/idecoder.h"
#include "fl/stl/noexcept.h"

namespace fl {

IDecoderPtr H264::createDecoder(const H264Config& config,
                                fl::string* error_message) FL_NOEXCEPT {
    FL_UNUSED(config);
    if (error_message) {
        *error_message = "H.264 decoding not supported on this platform";
    }
    return fl::make_shared<NullDecoder>();
}

bool H264::isSupported() { return false; }

} // namespace fl
