#pragma once

#include "enabled.h"

#if FASTLED_RMT5

#include "defs.h"
#include "rmt_strip.h"

namespace fastled_rmt51_strip {

// Acts a limiter on the number of active RMT channels.
// If there are more strips than RMT channels available
// this class will wait for a strip to finish before allowing
// a new one to start.
class RmtActiveStripGroup {
public:
    static RmtActiveStripGroup& instance();
    void add(IRmtLedStrip* strip);
    void remove(IRmtLedStrip* strip);
    void wait_for_any_strip_to_release();
    int count_active();
    void wait_if_max_number_active();
    void set_total_allowed(int value);
    int get_total_allowed() const;

private:
    // Way more than we should ever need now. This is used to
    // size the array of all possible RMT led strips.
    // One day an esp32 chip may actually have 64 RMT channels.
    static const int kMaxRmtLedStrips = 64;
    RmtActiveStripGroup();
    RmtActiveStripGroup(const RmtActiveStripGroup&) = delete;
    RmtActiveStripGroup& operator=(const RmtActiveStripGroup&) = delete;
    int mTotalActiveStripsAllowed = FASTLED_RMT_MAX_CHANNELS;  // Default -1 to allow runtime discovery.
    IRmtLedStrip* mAllRmtLedStrips[kMaxRmtLedStrips];
};

}  // namespace fastled_rmt51_strip

#endif // FASTLED_RMT5
