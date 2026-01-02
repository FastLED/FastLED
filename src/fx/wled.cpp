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

    // Extract "transition" field (0-65535) - optional, keep existing if missing
    if (wledState.contains("transition")) {
        if (wledState["transition"].is_int()) {
            int64_t transInt = wledState["transition"] | static_cast<int64_t>(mWledTransition);
            // Clamp to valid range 0-65535
            if (transInt < 0) {
                FL_WARN("WLED: transition " << transInt << " out of range, clamping to 0");
                transInt = 0;
            } else if (transInt > 65535) {
                FL_WARN("WLED: transition " << transInt << " out of range, clamping to 65535");
                transInt = 65535;
            }
            uint16_t newTrans = static_cast<uint16_t>(transInt);
            if (newTrans != mWledTransition) {
                mWledTransition = newTrans;
                FL_DBG("WLED: transition=" << mWledTransition);
            }
        } else {
            FL_WARN("WLED: 'transition' field has invalid type (expected int)");
        }
    }

    // Extract "ps" field (preset, -1 to 250) - optional, keep existing if missing
    if (wledState.contains("ps")) {
        if (wledState["ps"].is_int()) {
            int64_t psInt = wledState["ps"] | static_cast<int64_t>(mWledPreset);
            // Clamp to valid range -1 to 250
            if (psInt < -1) {
                FL_WARN("WLED: preset " << psInt << " out of range, clamping to -1");
                psInt = -1;
            } else if (psInt > 250) {
                FL_WARN("WLED: preset " << psInt << " out of range, clamping to 250");
                psInt = 250;
            }
            int16_t newPreset = static_cast<int16_t>(psInt);
            if (newPreset != mWledPreset) {
                mWledPreset = newPreset;
                FL_DBG("WLED: ps=" << mWledPreset);
            }
        } else {
            FL_WARN("WLED: 'ps' field has invalid type (expected int)");
        }
    }

    // Extract "pl" field (playlist, -1 to 250) - optional, keep existing if missing
    if (wledState.contains("pl")) {
        if (wledState["pl"].is_int()) {
            int64_t plInt = wledState["pl"] | static_cast<int64_t>(mWledPlaylist);
            // Clamp to valid range -1 to 250
            if (plInt < -1) {
                FL_WARN("WLED: playlist " << plInt << " out of range, clamping to -1");
                plInt = -1;
            } else if (plInt > 250) {
                FL_WARN("WLED: playlist " << plInt << " out of range, clamping to 250");
                plInt = 250;
            }
            int16_t newPlaylist = static_cast<int16_t>(plInt);
            if (newPlaylist != mWledPlaylist) {
                mWledPlaylist = newPlaylist;
                FL_DBG("WLED: pl=" << mWledPlaylist);
            }
        } else {
            FL_WARN("WLED: 'pl' field has invalid type (expected int)");
        }
    }

    // Extract "lor" field (live override, 0-2) - optional, keep existing if missing
    if (wledState.contains("lor")) {
        if (wledState["lor"].is_int()) {
            int64_t lorInt = wledState["lor"] | static_cast<int64_t>(mWledLiveOverride);
            // Clamp to valid range 0-2
            if (lorInt < 0) {
                FL_WARN("WLED: live override " << lorInt << " out of range, clamping to 0");
                lorInt = 0;
            } else if (lorInt > 2) {
                FL_WARN("WLED: live override " << lorInt << " out of range, clamping to 2");
                lorInt = 2;
            }
            uint8_t newLor = static_cast<uint8_t>(lorInt);
            if (newLor != mWledLiveOverride) {
                mWledLiveOverride = newLor;
                FL_DBG("WLED: lor=" << static_cast<int>(mWledLiveOverride));
            }
        } else {
            FL_WARN("WLED: 'lor' field has invalid type (expected int)");
        }
    }

    // Extract "mainseg" field (main segment, 0-maxseg-1) - optional, keep existing if missing
    if (wledState.contains("mainseg")) {
        if (wledState["mainseg"].is_int()) {
            int64_t mainsegInt = wledState["mainseg"] | static_cast<int64_t>(mWledMainSeg);
            // Clamp to valid range 0-255 (reasonable upper bound)
            if (mainsegInt < 0) {
                FL_WARN("WLED: mainseg " << mainsegInt << " out of range, clamping to 0");
                mainsegInt = 0;
            } else if (mainsegInt > 255) {
                FL_WARN("WLED: mainseg " << mainsegInt << " out of range, clamping to 255");
                mainsegInt = 255;
            }
            uint8_t newMainseg = static_cast<uint8_t>(mainsegInt);
            if (newMainseg != mWledMainSeg) {
                mWledMainSeg = newMainseg;
                FL_DBG("WLED: mainseg=" << static_cast<int>(mWledMainSeg));
            }
        } else {
            FL_WARN("WLED: 'mainseg' field has invalid type (expected int)");
        }
    }

    // Extract "nl" field (nightlight object) - optional
    if (wledState.contains("nl")) {
        if (wledState["nl"].is_object()) {
            const fl::Json& nl = wledState["nl"];

            // Extract "on" field (bool)
            if (nl.contains("on") && nl["on"].is_bool()) {
                bool newNlOn = nl["on"] | mWledNightlightOn;
                if (newNlOn != mWledNightlightOn) {
                    mWledNightlightOn = newNlOn;
                    FL_DBG("WLED: nl.on=" << (mWledNightlightOn ? "true" : "false"));
                }
            }

            // Extract "dur" field (1-255 minutes)
            if (nl.contains("dur")) {
                if (nl["dur"].is_int()) {
                    int64_t durInt = nl["dur"] | static_cast<int64_t>(mWledNightlightDur);
                    // Clamp to valid range 1-255
                    if (durInt < 1) {
                        FL_WARN("WLED: nl.dur " << durInt << " out of range, clamping to 1");
                        durInt = 1;
                    } else if (durInt > 255) {
                        FL_WARN("WLED: nl.dur " << durInt << " out of range, clamping to 255");
                        durInt = 255;
                    }
                    uint8_t newDur = static_cast<uint8_t>(durInt);
                    if (newDur != mWledNightlightDur) {
                        mWledNightlightDur = newDur;
                        FL_DBG("WLED: nl.dur=" << static_cast<int>(mWledNightlightDur));
                    }
                } else {
                    FL_WARN("WLED: 'nl.dur' field has invalid type (expected int)");
                }
            }

            // Extract "mode" field (0-3)
            if (nl.contains("mode")) {
                if (nl["mode"].is_int()) {
                    int64_t modeInt = nl["mode"] | static_cast<int64_t>(mWledNightlightMode);
                    // Clamp to valid range 0-3
                    if (modeInt < 0) {
                        FL_WARN("WLED: nl.mode " << modeInt << " out of range, clamping to 0");
                        modeInt = 0;
                    } else if (modeInt > 3) {
                        FL_WARN("WLED: nl.mode " << modeInt << " out of range, clamping to 3");
                        modeInt = 3;
                    }
                    uint8_t newMode = static_cast<uint8_t>(modeInt);
                    if (newMode != mWledNightlightMode) {
                        mWledNightlightMode = newMode;
                        FL_DBG("WLED: nl.mode=" << static_cast<int>(mWledNightlightMode));
                    }
                } else {
                    FL_WARN("WLED: 'nl.mode' field has invalid type (expected int)");
                }
            }

            // Extract "tbri" field (0-255 target brightness)
            if (nl.contains("tbri")) {
                if (nl["tbri"].is_int()) {
                    int64_t tbriInt = nl["tbri"] | static_cast<int64_t>(mWledNightlightTbri);
                    // Clamp to valid range 0-255
                    if (tbriInt < 0) {
                        FL_WARN("WLED: nl.tbri " << tbriInt << " out of range, clamping to 0");
                        tbriInt = 0;
                    } else if (tbriInt > 255) {
                        FL_WARN("WLED: nl.tbri " << tbriInt << " out of range, clamping to 255");
                        tbriInt = 255;
                    }
                    uint8_t newTbri = static_cast<uint8_t>(tbriInt);
                    if (newTbri != mWledNightlightTbri) {
                        mWledNightlightTbri = newTbri;
                        FL_DBG("WLED: nl.tbri=" << static_cast<int>(mWledNightlightTbri));
                    }
                } else {
                    FL_WARN("WLED: 'nl.tbri' field has invalid type (expected int)");
                }
            }
        } else {
            FL_WARN("WLED: 'nl' field has invalid type (expected object)");
        }
    }
}

fl::Json WLED::getState() const {
    fl::Json state = fl::Json::object();
    state.set("on", mWledOn);
    state.set("bri", static_cast<int64_t>(mWledBri));
    state.set("transition", static_cast<int64_t>(mWledTransition));
    state.set("ps", static_cast<int64_t>(mWledPreset));
    state.set("pl", static_cast<int64_t>(mWledPlaylist));
    state.set("lor", static_cast<int64_t>(mWledLiveOverride));
    state.set("mainseg", static_cast<int64_t>(mWledMainSeg));

    // Nightlight object
    fl::Json nl = fl::Json::object();
    nl.set("on", mWledNightlightOn);
    nl.set("dur", static_cast<int64_t>(mWledNightlightDur));
    nl.set("mode", static_cast<int64_t>(mWledNightlightMode));
    nl.set("tbri", static_cast<int64_t>(mWledNightlightTbri));
    state.set("nl", nl);

    return state;
}

} // namespace fl

#endif // FASTLED_ENABLE_JSON
