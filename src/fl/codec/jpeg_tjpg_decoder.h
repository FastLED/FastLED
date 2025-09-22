#pragma once

#include "fl/codec/jpeg.h"
#include "third_party/TJpg_Decoder/src/TJpg_Decoder.h"

namespace fl {

class TJpgDecoder : public JpegDecoderBase {
private:
    fl::third_party::TJpg_Decoder decoder_;
    fl::scoped_array<fl::u8> inputBuffer_;
    size_t inputSize_;
    bool frameDecoded_;
    fl::shared_ptr<Frame> decodedFrame_;

    // Static callback for TJpg_Decoder output
    static bool outputCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* data);

    // Internal helper methods
    void mapQualityToScale();
    void convertPixelFormat(uint16_t* srcData, fl::u8* dstData, uint16_t w, uint16_t h);
    fl::size getBytesPerPixel() const;

protected:
    bool initializeDecoder() override;
    bool decodeInternal() override;
    void cleanupDecoder() override;
    void allocateFrameBuffer(uint16_t width, uint16_t height);

public:
    explicit TJpgDecoder(const JpegConfig& config);
    ~TJpgDecoder() override;

    // Override to return our decoded frame
    Frame getCurrentFrame() override;
};

} // namespace fl