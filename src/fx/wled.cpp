#include "fx/wled.h"

#if FASTLED_ENABLE_JSON

#include "fl/warn.h"
#include "fl/dbg.h"

namespace fl {

// WLED State Management

void WLED::setState(const fl::Json& wledState) {
    if (!wledState.has_value()) {
        FL_WARN("WLED: setState called with invalid JSON");
        return;
    }

    // Extract "on" field (bool) - optional, keep existing if missing
    if (wledState.contains("on") && wledState["on"].is_bool()) {
        bool newOn = wledState["on"] | mWledOn;
        if (newOn != mWledOn) {
            mWledOn = newOn;
            FL_DBG("WLED: on=" << (mWledOn ? "true" : "false"));
        }
    }

    // Extract "bri" field (0-255) - optional, keep existing if missing
    if (wledState.contains("bri")) {
        if (wledState["bri"].is_int()) {
            int64_t briInt = wledState["bri"] | static_cast<int64_t>(mWledBri);
            // Clamp to valid range 0-255
            if (briInt < 0) {
                FL_WARN("WLED: brightness " << briInt << " out of range, clamping to 0");
                briInt = 0;
            } else if (briInt > 255) {
                FL_WARN("WLED: brightness " << briInt << " out of range, clamping to 255");
                briInt = 255;
            }
            uint8_t newBri = static_cast<uint8_t>(briInt);
            if (newBri != mWledBri) {
                mWledBri = newBri;
                FL_DBG("WLED: bri=" << static_cast<int>(mWledBri));
            }
        } else {
            FL_WARN("WLED: 'bri' field has invalid type (expected int)");
        }
    }
}

fl::Json WLED::getState() const {
    fl::Json state = fl::Json::object();
    state.set("on", mWledOn);
    state.set("bri", static_cast<int64_t>(mWledBri));
    return state;
}

} // namespace fl

#endif // FASTLED_ENABLE_JSON
