

#include "enabled.h"

#if FASTLED_RMT5


#include "rmt_strip_group.h"

#include "rmt_strip.h"
#include "configure_led.h"
#include "construct.h"
#include "esp_check.h"


namespace fastled_rmt51_strip {

RmtActiveStripGroup& RmtActiveStripGroup::instance() {
    static RmtActiveStripGroup instance;
    return instance;
}

void RmtActiveStripGroup::add(IRmtLedStrip* strip) {
    for (int i = 0; i < kMaxRmtLedStrips; i++) {
        if (mAllRmtLedStrips[i] == nullptr) {
            mAllRmtLedStrips[i] = strip;
            return;
        }
    }
    ESP_ERROR_CHECK(ESP_ERR_NOT_FOUND);
}

void RmtActiveStripGroup::remove(IRmtLedStrip* strip) {
    int found_idx = -1;
    for (int i = 0; i < kMaxRmtLedStrips; i++) {
        if (mAllRmtLedStrips[i] == strip) {
            mAllRmtLedStrips[i] = nullptr;
            found_idx = i;
        }
    }
    if (found_idx == -1) {
        ESP_ERROR_CHECK(ESP_ERR_NOT_FOUND);
    }
    // We want no gaps in the array so that we can only wait on the oldest
    // element, which will be towards the front. Newest elements will be
    // towards the back.
    for (int i = found_idx; i < kMaxRmtLedStrips - 1; i++) {
        mAllRmtLedStrips[i] = mAllRmtLedStrips[i + 1];
    }
    mAllRmtLedStrips[kMaxRmtLedStrips - 1] = nullptr;
}

void RmtActiveStripGroup::wait_for_any_strip_to_release() {
    for (int i = 0; i < kMaxRmtLedStrips; i++) {
        if (mAllRmtLedStrips[i]) {
            mAllRmtLedStrips[i]->wait_for_draw_complete();
        }
    }
}

int RmtActiveStripGroup::count_active() {
    int count = 0;
    for (int i = 0; i < kMaxRmtLedStrips; i++) {
        if (mAllRmtLedStrips[i]) {
            count++;
        }
    }
    return count;
}

void RmtActiveStripGroup::wait_if_max_number_active() {
    if (mTotalActiveStripsAllowed == -1) {
        // We don't know the limit yet.
        return;
    }
    if (mTotalActiveStripsAllowed == 0) {
        // in invalid number of active strips. In this case we just abort
        // the program.
        ESP_ERROR_CHECK(ESP_FAIL);
    }
    // We've hit the limit before and now the number of known max number
    // active strips is known and that we are saturated. Therefore we block
    // the main thread until a strip is available.
    if (count_active() >= mTotalActiveStripsAllowed) {
        wait_for_any_strip_to_release();
    }
}

void RmtActiveStripGroup::set_total_allowed(int value) {
    mTotalActiveStripsAllowed = value;
}

int RmtActiveStripGroup::get_total_allowed() const {
    return mTotalActiveStripsAllowed;
}

RmtActiveStripGroup::RmtActiveStripGroup() {
    for (int i = 0; i < kMaxRmtLedStrips; i++) {
        mAllRmtLedStrips[i] = nullptr;
    }
}

}  // namespace fastled_rmt51_strip

#endif // FASTLED_RMT5