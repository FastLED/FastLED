// IWYU pragma: private
#pragma once

#include "platforms/esp/is_esp.h"

#if defined(FL_IS_ESP_32P4)

#include "fl/codec/idecoder.h"
#include "fl/stl/string.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/noexcept.h"

namespace fl {

// Hardware-accelerated H.264 decoder for ESP32-P4.
// Wraps the esp_h264 component from ESP-IDF.
class H264HwDecoder : public IDecoder {
public:
    H264HwDecoder() FL_NOEXCEPT;
    ~H264HwDecoder() override;

    bool begin(fl::filebuf_ptr stream) FL_NOEXCEPT override;
    void end() FL_NOEXCEPT override;
    bool isReady() const FL_NOEXCEPT override;
    bool hasError(fl::string* msg = nullptr) const FL_NOEXCEPT override;

    DecodeResult decode() FL_NOEXCEPT override;
    Frame getCurrentFrame() FL_NOEXCEPT override;
    bool hasMoreFrames() const FL_NOEXCEPT override;

private:
    struct Impl;
    fl::unique_ptr<Impl> mImpl;
};

} // namespace fl

#endif // FL_IS_ESP_32P4
