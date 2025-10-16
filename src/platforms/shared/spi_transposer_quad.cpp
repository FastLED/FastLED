#include "spi_transposer_quad.h"

namespace fl {

bool SPITransposerQuad::transpose(const fl::optional<LaneData>& lane0,
                                   const fl::optional<LaneData>& lane1,
                                   const fl::optional<LaneData>& lane2,
                                   const fl::optional<LaneData>& lane3,
                                   fl::span<uint8_t> output,
                                   const char** error) {
    // Delegate to unified SPITransposer implementation
    return SPITransposer::transpose4(lane0, lane1, lane2, lane3, output, error);
}

bool SPITransposerQuad::transpose8(const fl::optional<LaneData> lanes[8],
                                    fl::span<uint8_t> output,
                                    const char** error) {
    // Delegate to unified SPITransposer implementation
    return SPITransposer::transpose8(lanes, output, error);
}

}  // namespace fl
