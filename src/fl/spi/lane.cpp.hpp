#include "fl/spi/lane.h"
#include "fl/system/log.h"
#include "fl/system/log.h"

namespace fl {
namespace spi {

Lane::Lane(size_t lane_id, MultiLaneDevice* parent)
    : mLaneId(lane_id) {
    (void)parent;  // Unused parameter (reserved for future use)
}

void Lane::write(const u8* data, size_t size) {
    if (!data || size == 0) {
        FL_WARN("Lane " << mLaneId << ": Invalid data or size");
        return;
    }

    // Resize buffer to fit new data
    mBuffer.resize(size);

    // Copy data into buffer
    for (size_t i = 0; i < size; i++) {
        mBuffer[i] = data[i];
    }

    FL_DBG("Lane " << mLaneId << ": Buffered " << size << " bytes");
}

fl::span<u8> Lane::getBuffer(size_t size) {
    // Resize buffer to requested size
    mBuffer.resize(size);

    // Return span to buffer
    return mBuffer;
}

fl::span<const u8> Lane::data() const {
    return mBuffer;
}

} // namespace spi
} // namespace fl
