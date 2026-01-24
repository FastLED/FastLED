#include "fl/fx/wled.h"
#include "fl/warn.h"
#include "fl/dbg.h"

#if FASTLED_ENABLE_JSON

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
            int64_t transInt = wledState["transition"] | static_cast<int64_t>(mTransition);
            // Clamp to valid range 0-65535
            if (transInt < 0) {
                FL_WARN("WLED: transition " << transInt << " out of range, clamping to 0");
                transInt = 0;
            } else if (transInt > 65535) {
                FL_WARN("WLED: transition " << transInt << " out of range, clamping to 65535");
                transInt = 65535;
            }
            uint16_t newTransition = static_cast<uint16_t>(transInt);
            if (newTransition != mTransition) {
                mTransition = newTransition;
                FL_DBG("WLED: transition=" << mTransition);
            }
        } else {
            FL_WARN("WLED: 'transition' field has invalid type (expected int)");
        }
    }

    // Extract "ps" field (preset ID: -1 to 250) - optional, keep existing if missing
    if (wledState.contains("ps")) {
        if (wledState["ps"].is_int()) {
            int64_t psInt = wledState["ps"] | static_cast<int64_t>(mPreset);
            // Clamp to valid range -1 to 250
            if (psInt < -1) {
                FL_WARN("WLED: preset " << psInt << " out of range, clamping to -1");
                psInt = -1;
            } else if (psInt > 250) {
                FL_WARN("WLED: preset " << psInt << " out of range, clamping to 250");
                psInt = 250;
            }
            int16_t newPreset = static_cast<int16_t>(psInt);
            if (newPreset != mPreset) {
                mPreset = newPreset;
                FL_DBG("WLED: ps=" << mPreset);
            }
        } else {
            FL_WARN("WLED: 'ps' field has invalid type (expected int)");
        }
    }

    // Extract "pl" field (playlist ID: -1 to 250) - optional, keep existing if missing
    if (wledState.contains("pl")) {
        if (wledState["pl"].is_int()) {
            int64_t plInt = wledState["pl"] | static_cast<int64_t>(mPlaylist);
            // Clamp to valid range -1 to 250
            if (plInt < -1) {
                FL_WARN("WLED: playlist " << plInt << " out of range, clamping to -1");
                plInt = -1;
            } else if (plInt > 250) {
                FL_WARN("WLED: playlist " << plInt << " out of range, clamping to 250");
                plInt = 250;
            }
            int16_t newPlaylist = static_cast<int16_t>(plInt);
            if (newPlaylist != mPlaylist) {
                mPlaylist = newPlaylist;
                FL_DBG("WLED: pl=" << mPlaylist);
            }
        } else {
            FL_WARN("WLED: 'pl' field has invalid type (expected int)");
        }
    }

    // Extract "lor" field (live override: 0-2) - optional, keep existing if missing
    if (wledState.contains("lor")) {
        if (wledState["lor"].is_int()) {
            int64_t lorInt = wledState["lor"] | static_cast<int64_t>(mLiveOverride);
            // Clamp to valid range 0-2
            if (lorInt < 0) {
                FL_WARN("WLED: live override " << lorInt << " out of range, clamping to 0");
                lorInt = 0;
            } else if (lorInt > 2) {
                FL_WARN("WLED: live override " << lorInt << " out of range, clamping to 2");
                lorInt = 2;
            }
            uint8_t newLiveOverride = static_cast<uint8_t>(lorInt);
            if (newLiveOverride != mLiveOverride) {
                mLiveOverride = newLiveOverride;
                FL_DBG("WLED: lor=" << static_cast<int>(mLiveOverride));
            }
        } else {
            FL_WARN("WLED: 'lor' field has invalid type (expected int)");
        }
    }

    // Extract "mainseg" field (main segment: 0-255) - optional, keep existing if missing
    if (wledState.contains("mainseg")) {
        if (wledState["mainseg"].is_int()) {
            int64_t mainsegInt = wledState["mainseg"] | static_cast<int64_t>(mMainSegment);
            // Clamp to valid range 0-255
            if (mainsegInt < 0) {
                FL_WARN("WLED: main segment " << mainsegInt << " out of range, clamping to 0");
                mainsegInt = 0;
            } else if (mainsegInt > 255) {
                FL_WARN("WLED: main segment " << mainsegInt << " out of range, clamping to 255");
                mainsegInt = 255;
            }
            uint8_t newMainSegment = static_cast<uint8_t>(mainsegInt);
            if (newMainSegment != mMainSegment) {
                mMainSegment = newMainSegment;
                FL_DBG("WLED: mainseg=" << static_cast<int>(mMainSegment));
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
                bool newNlOn = nl["on"] | mNightlightOn;
                if (newNlOn != mNightlightOn) {
                    mNightlightOn = newNlOn;
                    FL_DBG("WLED: nl.on=" << (mNightlightOn ? "true" : "false"));
                }
            }

            // Extract "dur" field (1-255 minutes)
            if (nl.contains("dur")) {
                if (nl["dur"].is_int()) {
                    int64_t durInt = nl["dur"] | static_cast<int64_t>(mNightlightDuration);
                    // Clamp to valid range 1-255
                    if (durInt < 1) {
                        FL_WARN("WLED: nl.dur " << durInt << " out of range, clamping to 1");
                        durInt = 1;
                    } else if (durInt > 255) {
                        FL_WARN("WLED: nl.dur " << durInt << " out of range, clamping to 255");
                        durInt = 255;
                    }
                    uint8_t newDur = static_cast<uint8_t>(durInt);
                    if (newDur != mNightlightDuration) {
                        mNightlightDuration = newDur;
                        FL_DBG("WLED: nl.dur=" << static_cast<int>(mNightlightDuration));
                    }
                } else {
                    FL_WARN("WLED: 'nl.dur' field has invalid type (expected int)");
                }
            }

            // Extract "mode" field (0-3)
            if (nl.contains("mode")) {
                if (nl["mode"].is_int()) {
                    int64_t modeInt = nl["mode"] | static_cast<int64_t>(mNightlightMode);
                    // Clamp to valid range 0-3
                    if (modeInt < 0) {
                        FL_WARN("WLED: nl.mode " << modeInt << " out of range, clamping to 0");
                        modeInt = 0;
                    } else if (modeInt > 3) {
                        FL_WARN("WLED: nl.mode " << modeInt << " out of range, clamping to 3");
                        modeInt = 3;
                    }
                    uint8_t newMode = static_cast<uint8_t>(modeInt);
                    if (newMode != mNightlightMode) {
                        mNightlightMode = newMode;
                        FL_DBG("WLED: nl.mode=" << static_cast<int>(mNightlightMode));
                    }
                } else {
                    FL_WARN("WLED: 'nl.mode' field has invalid type (expected int)");
                }
            }

            // Extract "tbri" field (0-255 target brightness)
            if (nl.contains("tbri")) {
                if (nl["tbri"].is_int()) {
                    int64_t tbriInt = nl["tbri"] | static_cast<int64_t>(mNightlightTargetBrightness);
                    // Clamp to valid range 0-255
                    if (tbriInt < 0) {
                        FL_WARN("WLED: nl.tbri " << tbriInt << " out of range, clamping to 0");
                        tbriInt = 0;
                    } else if (tbriInt > 255) {
                        FL_WARN("WLED: nl.tbri " << tbriInt << " out of range, clamping to 255");
                        tbriInt = 255;
                    }
                    uint8_t newTbri = static_cast<uint8_t>(tbriInt);
                    if (newTbri != mNightlightTargetBrightness) {
                        mNightlightTargetBrightness = newTbri;
                        FL_DBG("WLED: nl.tbri=" << static_cast<int>(mNightlightTargetBrightness));
                    }
                } else {
                    FL_WARN("WLED: 'nl.tbri' field has invalid type (expected int)");
                }
            }
        } else {
            FL_WARN("WLED: 'nl' field has invalid type (expected object)");
        }
    }

    // Extract "udpn" field (UDP sync settings) - optional
    if (wledState.contains("udpn")) {
        if (wledState["udpn"].is_object()) {
            const fl::Json& udpn = wledState["udpn"];

            // Extract "send" field (bool)
            if (udpn.contains("send") && udpn["send"].is_bool()) {
                bool newSend = udpn["send"] | mUdpSend;
                if (newSend != mUdpSend) {
                    mUdpSend = newSend;
                    FL_DBG("WLED: udpn.send=" << (mUdpSend ? "true" : "false"));
                }
            }

            // Extract "recv" field (bool)
            if (udpn.contains("recv") && udpn["recv"].is_bool()) {
                bool newRecv = udpn["recv"] | mUdpReceive;
                if (newRecv != mUdpReceive) {
                    mUdpReceive = newRecv;
                    FL_DBG("WLED: udpn.recv=" << (mUdpReceive ? "true" : "false"));
                }
            }
        } else {
            FL_WARN("WLED: 'udpn' field has invalid type (expected object)");
        }
    }

    // Extract "playlist" field (playlist configuration) - optional
    if (wledState.contains("playlist")) {
        if (wledState["playlist"].is_object()) {
            const fl::Json& pl = wledState["playlist"];

            // Extract "ps" field (array of preset IDs)
            if (pl.contains("ps") && pl["ps"].is_array()) {
                mPlaylistPresets.clear();
                for (size_t i = 0; i < pl["ps"].size(); i++) {
                    if (pl["ps"][i].is_int()) {
                        int64_t psInt = pl["ps"][i] | -1;
                        // Clamp to valid range -1 to 250
                        if (psInt < -1) psInt = -1;
                        if (psInt > 250) psInt = 250;
                        mPlaylistPresets.push_back(static_cast<int16_t>(psInt));
                    }
                }
                FL_DBG("WLED: playlist.ps count=" << mPlaylistPresets.size());
            }

            // Extract "dur" field (array of durations in seconds)
            if (pl.contains("dur") && pl["dur"].is_array()) {
                mPlaylistDurations.clear();
                for (size_t i = 0; i < pl["dur"].size(); i++) {
                    if (pl["dur"][i].is_int()) {
                        int64_t durInt = pl["dur"][i] | 0;
                        if (durInt < 0) durInt = 0;
                        if (durInt > 65535) durInt = 65535;
                        mPlaylistDurations.push_back(static_cast<uint16_t>(durInt));
                    }
                }
            }

            // Extract "transition" field (array of transitions in ×100ms)
            if (pl.contains("transition") && pl["transition"].is_array()) {
                mPlaylistTransitions.clear();
                for (size_t i = 0; i < pl["transition"].size(); i++) {
                    if (pl["transition"][i].is_int()) {
                        int64_t transInt = pl["transition"][i] | 0;
                        if (transInt < 0) transInt = 0;
                        if (transInt > 65535) transInt = 65535;
                        mPlaylistTransitions.push_back(static_cast<uint16_t>(transInt));
                    }
                }
            }

            // Extract "repeat" field
            if (pl.contains("repeat") && pl["repeat"].is_int()) {
                int64_t repeatInt = pl["repeat"] | 0;
                if (repeatInt < 0) repeatInt = 0;
                if (repeatInt > 65535) repeatInt = 65535;
                mPlaylistRepeat = static_cast<uint16_t>(repeatInt);
                FL_DBG("WLED: playlist.repeat=" << mPlaylistRepeat);
            }

            // Extract "end" field
            if (pl.contains("end") && pl["end"].is_int()) {
                int64_t endInt = pl["end"] | -1;
                if (endInt < -1) endInt = -1;
                if (endInt > 250) endInt = 250;
                mPlaylistEnd = static_cast<int16_t>(endInt);
                FL_DBG("WLED: playlist.end=" << mPlaylistEnd);
            }

            // Extract "r" field (randomize)
            if (pl.contains("r") && pl["r"].is_bool()) {
                mPlaylistRandomize = pl["r"] | false;
                FL_DBG("WLED: playlist.r=" << (mPlaylistRandomize ? "true" : "false"));
            }
        } else {
            FL_WARN("WLED: 'playlist' field has invalid type (expected object)");
        }
    }

    // Extract "seg" field (segment array) - optional
    if (wledState.contains("seg")) {
        if (wledState["seg"].is_array()) {
            for (size_t i = 0; i < wledState["seg"].size(); i++) {
                const fl::Json& segJson = wledState["seg"][i];
                if (!segJson.is_object()) {
                    FL_WARN("WLED: segment at index " << i << " is not an object");
                    continue;
                }

                // Extract segment ID (required for updates)
                uint8_t segId = 0;
                if (segJson.contains("id") && segJson["id"].is_int()) {
                    int64_t idInt = segJson["id"] | 0;
                    if (idInt < 0) idInt = 0;
                    if (idInt > 255) idInt = 255;
                    segId = static_cast<uint8_t>(idInt);
                } else {
                    // If no ID provided, use index
                    segId = static_cast<uint8_t>(i);
                }

                // Find existing segment or create new one
                WLEDSegment* seg = nullptr;
                for (size_t j = 0; j < mSegments.size(); j++) {
                    if (mSegments[j].mId == segId) {
                        seg = &mSegments[j];
                        break;
                    }
                }
                if (!seg) {
                    // Create new segment
                    WLEDSegment newSeg;
                    newSeg.mId = segId;
                    mSegments.push_back(newSeg);
                    seg = &mSegments[mSegments.size() - 1];
                }

                // Parse segment fields using helper function
                wled::parseSegmentFields(segJson, *seg);
            }
        } else {
            FL_WARN("WLED: 'seg' field has invalid type (expected array)");
        }
    }
}

fl::Json WLED::getState() const {
    fl::Json state = fl::Json::object();
    state.set("on", mWledOn);
    state.set("bri", static_cast<int64_t>(mWledBri));
    state.set("transition", static_cast<int64_t>(mTransition));
    state.set("ps", static_cast<int64_t>(mPreset));
    state.set("pl", static_cast<int64_t>(mPlaylist));
    state.set("lor", static_cast<int64_t>(mLiveOverride));
    state.set("mainseg", static_cast<int64_t>(mMainSegment));

    // Nightlight object
    fl::Json nl = fl::Json::object();
    nl.set("on", mNightlightOn);
    nl.set("dur", static_cast<int64_t>(mNightlightDuration));
    nl.set("mode", static_cast<int64_t>(mNightlightMode));
    nl.set("tbri", static_cast<int64_t>(mNightlightTargetBrightness));
    state.set("nl", nl);

    // UDP sync settings
    fl::Json udpn = fl::Json::object();
    udpn.set("send", mUdpSend);
    udpn.set("recv", mUdpReceive);
    state.set("udpn", udpn);

    // Playlist configuration (if present)
    if (hasPlaylistConfig()) {
        state.set("playlist", getPlaylistConfig());
    }

    // Segments
    if (!mSegments.empty()) {
        fl::Json segments = fl::Json::array();
        for (const auto& seg : mSegments) {
            fl::Json segJson = fl::Json::object();

            segJson.set("id", static_cast<int64_t>(seg.mId));
            segJson.set("start", static_cast<int64_t>(seg.mStart));
            segJson.set("stop", static_cast<int64_t>(seg.mStop));
            segJson.set("len", static_cast<int64_t>(seg.mLen));
            segJson.set("grp", static_cast<int64_t>(seg.mGrp));
            segJson.set("spc", static_cast<int64_t>(seg.mSpc));
            segJson.set("of", static_cast<int64_t>(seg.mOf));
            segJson.set("on", seg.mOn);
            segJson.set("bri", static_cast<int64_t>(seg.mBri));
            segJson.set("cct", static_cast<int64_t>(seg.mCct));

            // Effect properties
            segJson.set("fx", static_cast<int64_t>(seg.mFx));
            segJson.set("sx", static_cast<int64_t>(seg.mSx));
            segJson.set("ix", static_cast<int64_t>(seg.mIx));
            segJson.set("pal", static_cast<int64_t>(seg.mPal));
            segJson.set("c1", static_cast<int64_t>(seg.mC1));
            segJson.set("c2", static_cast<int64_t>(seg.mC2));
            segJson.set("c3", static_cast<int64_t>(seg.mC3));

            // Boolean flags
            segJson.set("sel", seg.mSel);
            segJson.set("rev", seg.mRev);
            segJson.set("mi", seg.mMi);
            segJson.set("o1", seg.mO1);
            segJson.set("o2", seg.mO2);
            segJson.set("o3", seg.mO3);
            segJson.set("si", static_cast<int64_t>(seg.mSi));
            segJson.set("m12", static_cast<int64_t>(seg.mM12));
            segJson.set("rpt", seg.mRpt);

            if (!seg.mName.empty()) {
                segJson.set("n", seg.mName);
            }

            // Colors
            if (!seg.mColors.empty()) {
                fl::Json colors = fl::Json::array();
                for (const auto& color : seg.mColors) {
                    if (color.size() >= 3) {
                        fl::Json colorArray = fl::Json::array();
                        for (size_t i = 0; i < color.size(); i++) {
                            colorArray.push_back(fl::Json(static_cast<int64_t>(color[i])));
                        }
                        colors.push_back(colorArray);
                    }
                }
                segJson.set("col", colors);
            }

            // Individual LEDs
            if (!seg.mIndividualLeds.empty()) {
                fl::Json leds = fl::Json::array();
                for (const auto& led : seg.mIndividualLeds) {
                    if (led.size() >= 3) {
                        fl::string hexColor = wled::rgbToHex(led[0], led[1], led[2]);
                        leds.push_back(fl::Json(hexColor.c_str()));
                    }
                }
                segJson.set("i", leds);
            }

            segments.push_back(segJson);
        }
        state.set("seg", segments);
    }

    return state;
}

fl::Json WLED::getPlaylistConfig() const {
    fl::Json playlist = fl::Json::object();

    // Preset IDs
    if (!mPlaylistPresets.empty()) {
        fl::Json ps = fl::Json::array();
        for (const auto& preset : mPlaylistPresets) {
            ps.push_back(fl::Json(static_cast<int64_t>(preset)));
        }
        playlist.set("ps", ps);
    }

    // Durations
    if (!mPlaylistDurations.empty()) {
        fl::Json dur = fl::Json::array();
        for (const auto& duration : mPlaylistDurations) {
            dur.push_back(fl::Json(static_cast<int64_t>(duration)));
        }
        playlist.set("dur", dur);
    }

    // Transitions
    if (!mPlaylistTransitions.empty()) {
        fl::Json trans = fl::Json::array();
        for (const auto& transition : mPlaylistTransitions) {
            trans.push_back(fl::Json(static_cast<int64_t>(transition)));
        }
        playlist.set("transition", trans);
    }

    // Other properties
    playlist.set("repeat", static_cast<int64_t>(mPlaylistRepeat));
    playlist.set("end", static_cast<int64_t>(mPlaylistEnd));
    playlist.set("r", mPlaylistRandomize);

    return playlist;
}

const WLEDSegment* WLED::findSegmentById(uint8_t id) const {
    for (const auto& seg : mSegments) {
        if (seg.mId == id) {
            return &seg;
        }
    }
    return nullptr;
}

} // namespace fl

#endif // FASTLED_ENABLE_JSON
