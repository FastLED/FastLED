#include "spi_transposer_dual.h"

namespace fl {

bool SPITransposerDual::transpose(const fl::optional<LaneData>& lane0,
                                   const fl::optional<LaneData>& lane1,
                                   fl::span<uint8_t> output,
                                   const char** error) {
    // Delegate to unified SPITransposer implementation
    return SPITransposer::transpose2(lane0, lane1, output, error);
}

}  // namespace fl
