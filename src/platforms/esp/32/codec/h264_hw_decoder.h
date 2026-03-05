// IWYU pragma: private
#pragma once

#include "platforms/esp/is_esp.h"

#if defined(FL_IS_ESP_32P4)

#include "fl/codec/idecoder.h"
#include "fl/stl/string.h"
#include "fl/stl/unique_ptr.h"

namespace fl {

// Hardware-accelerated H.264 decoder for ESP32-P4.
// Wraps the esp_h264 component from ESP-IDF.
class H264HwDecoder : public IDecoder {
public:
    H264HwDecoder();
    ~H264HwDecoder() override;

    bool begin(fl::FileHandlePtr stream) override;
    void end() override;
    bool isReady() const override;
    bool hasError(fl::string* msg = nullptr) const override;

    DecodeResult decode() override;
    Frame getCurrentFrame() override;
    bool hasMoreFrames() const override;

private:
    struct Impl;
    fl::unique_ptr<Impl> mImpl;
};

} // namespace fl

#endif // FL_IS_ESP_32P4
