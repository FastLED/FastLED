#include "fx/wled.h"
#include "FastLED.h"
#include "fl/warn.h"
#include "fl/dbg.h"

#if FASTLED_ENABLE_JSON

#include "fl/stl/cstdlib.h"  // For strtol

namespace fl {

// Forward declarations
static bool parseHexColor(const fl::string& hexStr, uint8_t& r, uint8_t& g, uint8_t& b);
static void parseSegmentFields(const fl::Json& segJson, WLEDSegment& seg);

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

                // Parse segment fields...
                parseSegmentFields(segJson, *seg);
            }
        } else {
            FL_WARN("WLED: 'seg' field has invalid type (expected array)");
        }
    }
}

// Helper function to parse hex color string to RGB
static bool parseHexColor(const fl::string& hexStr, uint8_t& r, uint8_t& g, uint8_t& b) {
    fl::string hex = hexStr;

    // Strip leading '#' if present
    if (hex.length() > 0 && hex[0] == '#') {
        hex = hex.substr(1);
    }

    // Validate length (must be exactly 6 characters)
    if (hex.length() != 6) {
        return false;
    }

    // Validate hex characters
    for (size_t i = 0; i < hex.length(); i++) {
        char c = hex[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            return false;
        }
    }

    // Parse hex digits
    auto hexDigitToInt = [](char c) -> uint8_t {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 0;
    };

    r = (hexDigitToInt(hex[0]) << 4) | hexDigitToInt(hex[1]);
    g = (hexDigitToInt(hex[2]) << 4) | hexDigitToInt(hex[3]);
    b = (hexDigitToInt(hex[4]) << 4) | hexDigitToInt(hex[5]);

    return true;
}

// Helper function to parse segment fields
static void parseSegmentFields(const fl::Json& segJson, WLEDSegment& seg) {
    // Extract basic properties
    if (segJson.contains("start") && segJson["start"].is_int()) {
        int64_t startInt = segJson["start"] | 0;
        if (startInt < 0) startInt = 0;
        if (startInt > 65535) startInt = 65535;
        seg.mStart = static_cast<uint16_t>(startInt);
    }

    if (segJson.contains("stop") && segJson["stop"].is_int()) {
        int64_t stopInt = segJson["stop"] | 0;
        if (stopInt < 0) stopInt = 0;
        if (stopInt > 65535) stopInt = 65535;
        seg.mStop = static_cast<uint16_t>(stopInt);
    }

    if (segJson.contains("len") && segJson["len"].is_int()) {
        int64_t lenInt = segJson["len"] | 0;
        if (lenInt < 0) lenInt = 0;
        if (lenInt > 65535) lenInt = 65535;
        seg.mLen = static_cast<uint16_t>(lenInt);
    }

    if (segJson.contains("grp") && segJson["grp"].is_int()) {
        int64_t grpInt = segJson["grp"] | 1;
        if (grpInt < 1) grpInt = 1;
        if (grpInt > 255) grpInt = 255;
        seg.mGrp = static_cast<uint8_t>(grpInt);
    }

    if (segJson.contains("spc") && segJson["spc"].is_int()) {
        int64_t spcInt = segJson["spc"] | 0;
        if (spcInt < 0) spcInt = 0;
        if (spcInt > 255) spcInt = 255;
        seg.mSpc = static_cast<uint8_t>(spcInt);
    }

    if (segJson.contains("of") && segJson["of"].is_int()) {
        int64_t ofInt = segJson["of"] | 0;
        if (ofInt < 0) ofInt = 0;
        if (ofInt > 65535) ofInt = 65535;
        seg.mOf = static_cast<uint16_t>(ofInt);
    }

    if (segJson.contains("on") && segJson["on"].is_bool()) {
        seg.mOn = segJson["on"] | true;
    }

    if (segJson.contains("bri") && segJson["bri"].is_int()) {
        int64_t briInt = segJson["bri"] | 255;
        if (briInt < 0) briInt = 0;
        if (briInt > 255) briInt = 255;
        seg.mBri = static_cast<uint8_t>(briInt);
    }

    if (segJson.contains("cct") && segJson["cct"].is_int()) {
        int64_t cctInt = segJson["cct"] | 0;
        if (cctInt < 0) cctInt = 0;
        if (cctInt > 65535) cctInt = 65535;
        seg.mCct = static_cast<uint16_t>(cctInt);
    }

    // Effect properties
    if (segJson.contains("fx") && segJson["fx"].is_int()) {
        int64_t fxInt = segJson["fx"] | 0;
        if (fxInt < 0) fxInt = 0;
        if (fxInt > 255) fxInt = 255;
        seg.mFx = static_cast<uint8_t>(fxInt);
    }

    if (segJson.contains("sx") && segJson["sx"].is_int()) {
        int64_t sxInt = segJson["sx"] | 128;
        if (sxInt < 0) sxInt = 0;
        if (sxInt > 255) sxInt = 255;
        seg.mSx = static_cast<uint8_t>(sxInt);
    }

    if (segJson.contains("ix") && segJson["ix"].is_int()) {
        int64_t ixInt = segJson["ix"] | 128;
        if (ixInt < 0) ixInt = 0;
        if (ixInt > 255) ixInt = 255;
        seg.mIx = static_cast<uint8_t>(ixInt);
    }

    if (segJson.contains("pal") && segJson["pal"].is_int()) {
        int64_t palInt = segJson["pal"] | 0;
        if (palInt < 0) palInt = 0;
        if (palInt > 255) palInt = 255;
        seg.mPal = static_cast<uint8_t>(palInt);
    }

    if (segJson.contains("c1") && segJson["c1"].is_int()) {
        int64_t c1Int = segJson["c1"] | 128;
        if (c1Int < 0) c1Int = 0;
        if (c1Int > 255) c1Int = 255;
        seg.mC1 = static_cast<uint8_t>(c1Int);
    }

    if (segJson.contains("c2") && segJson["c2"].is_int()) {
        int64_t c2Int = segJson["c2"] | 128;
        if (c2Int < 0) c2Int = 0;
        if (c2Int > 255) c2Int = 255;
        seg.mC2 = static_cast<uint8_t>(c2Int);
    }

    if (segJson.contains("c3") && segJson["c3"].is_int()) {
        int64_t c3Int = segJson["c3"] | 16;
        if (c3Int < 0) c3Int = 0;
        if (c3Int > 255) c3Int = 255;
        seg.mC3 = static_cast<uint8_t>(c3Int);
    }

    // Boolean flags
    if (segJson.contains("sel") && segJson["sel"].is_bool()) {
        seg.mSel = segJson["sel"] | false;
    }

    if (segJson.contains("rev") && segJson["rev"].is_bool()) {
        seg.mRev = segJson["rev"] | false;
    }

    if (segJson.contains("mi") && segJson["mi"].is_bool()) {
        seg.mMi = segJson["mi"] | false;
    }

    if (segJson.contains("o1") && segJson["o1"].is_bool()) {
        seg.mO1 = segJson["o1"] | false;
    }

    if (segJson.contains("o2") && segJson["o2"].is_bool()) {
        seg.mO2 = segJson["o2"] | false;
    }

    if (segJson.contains("o3") && segJson["o3"].is_bool()) {
        seg.mO3 = segJson["o3"] | false;
    }

    if (segJson.contains("si") && segJson["si"].is_int()) {
        int64_t siInt = segJson["si"] | 0;
        if (siInt < 0) siInt = 0;
        if (siInt > 3) siInt = 3;
        seg.mSi = static_cast<uint8_t>(siInt);
    }

    if (segJson.contains("m12") && segJson["m12"].is_int()) {
        int64_t m12Int = segJson["m12"] | 0;
        if (m12Int < 0) m12Int = 0;
        if (m12Int > 3) m12Int = 3;
        seg.mM12 = static_cast<uint8_t>(m12Int);
    }

    if (segJson.contains("rpt") && segJson["rpt"].is_bool()) {
        seg.mRpt = segJson["rpt"] | false;
    }

    if (segJson.contains("n") && segJson["n"].is_string()) {
        seg.mName = segJson["n"] | fl::string("");
    }

    // Parse "col" field (color slots)
    if (segJson.contains("col") && segJson["col"].is_array()) {
        seg.mColors.clear();
        for (size_t i = 0; i < segJson["col"].size(); i++) {
            const fl::Json& colJson = segJson["col"][i];

            if (colJson.is_array()) {
                // RGB(W) array format: [R,G,B] or [R,G,B,W]
                fl::vector<uint8_t> color;
                for (size_t j = 0; j < colJson.size() && j < 4; j++) {
                    if (colJson[j].is_int()) {
                        int64_t val = colJson[j] | 0;
                        if (val < 0) val = 0;
                        if (val > 255) val = 255;
                        color.push_back(static_cast<uint8_t>(val));
                    }
                }
                if (color.size() >= 3) {
                    seg.mColors.push_back(color);
                }
            } else if (colJson.is_string()) {
                // Hex string format: "RRGGBB" or "#RRGGBB"
                fl::string hexStr = colJson | fl::string("");
                uint8_t r, g, b;
                if (parseHexColor(hexStr, r, g, b)) {
                    fl::vector<uint8_t> color;
                    color.push_back(r);
                    color.push_back(g);
                    color.push_back(b);
                    seg.mColors.push_back(color);
                } else {
                    FL_WARN("WLED: invalid hex color string: " << hexStr);
                }
            }
        }
    }

    // Parse "i" field (individual LED control)
    if (segJson.contains("i") && segJson["i"].is_array()) {
        seg.mIndividualLeds.clear();

        size_t ledIndex = 0;
        for (size_t i = 0; i < segJson["i"].size(); i++) {
            const fl::Json& ledJson = segJson["i"][i];

            if (!ledJson.is_string()) {
                continue;
            }

            fl::string ledStr = ledJson | fl::string("");

            // Check for indexed or range format: "RRGGBB|index" or "RRGGBB|start-end"
            size_t pipePos = ledStr.find('|');

            fl::string hexStr;
            size_t startIdx = ledIndex;
            size_t endIdx = ledIndex;

            if (pipePos != fl::string::npos) {
                // Has index/range specifier
                hexStr = ledStr.substr(0, pipePos);
                fl::string indexStr = ledStr.substr(pipePos + 1);

                // Check for range: "start-end"
                size_t dashPos = indexStr.find('-');
                if (dashPos != fl::string::npos) {
                    // Range format
                    fl::string startStr = indexStr.substr(0, dashPos);
                    fl::string endStr = indexStr.substr(dashPos + 1);

                    // Parse start and end indices
                    char* endptr;
                    long startVal = strtol(startStr.c_str(), &endptr, 10);
                    if (endptr == startStr.c_str() || startVal < 0) {
                        FL_WARN("WLED: invalid range start index: " << startStr);
                        continue;
                    }

                    long endVal = strtol(endStr.c_str(), &endptr, 10);
                    if (endptr == endStr.c_str() || endVal < 0) {
                        FL_WARN("WLED: invalid range end index: " << endStr);
                        continue;
                    }

                    startIdx = static_cast<size_t>(startVal);
                    endIdx = static_cast<size_t>(endVal);
                } else {
                    // Single index format
                    char* endptr;
                    long idxVal = strtol(indexStr.c_str(), &endptr, 10);
                    if (endptr == indexStr.c_str() || idxVal < 0) {
                        FL_WARN("WLED: invalid LED index: " << indexStr);
                        continue;
                    }
                    startIdx = endIdx = static_cast<size_t>(idxVal);
                }
            } else {
                // Sequential format (no index specifier)
                hexStr = ledStr;
                ledIndex++;
            }

            // Parse hex color
            uint8_t r, g, b;
            if (!parseHexColor(hexStr, r, g, b)) {
                FL_WARN("WLED: invalid hex color in individual LED: " << hexStr);
                continue;
            }

            // Ensure array is large enough
            size_t maxIdx = (endIdx > startIdx) ? endIdx : startIdx;
            if (seg.mIndividualLeds.size() <= maxIdx) {
                seg.mIndividualLeds.resize(maxIdx + 1);
            }

            // Set LEDs in range
            for (size_t idx = startIdx; idx <= endIdx; idx++) {
                seg.mIndividualLeds[idx].clear();
                seg.mIndividualLeds[idx].push_back(r);
                seg.mIndividualLeds[idx].push_back(g);
                seg.mIndividualLeds[idx].push_back(b);
            }
        }
    }
}

// Helper function to convert RGB to hex string
static fl::string rgbToHex(uint8_t r, uint8_t g, uint8_t b) {
    fl::string hex;
    hex.reserve(6);

    auto byteToHex = [](uint8_t val) -> fl::string {
        const char hexChars[] = "0123456789ABCDEF";
        fl::string result;
        result += hexChars[val >> 4];
        result += hexChars[val & 0x0F];
        return result;
    };

    hex += byteToHex(r);
    hex += byteToHex(g);
    hex += byteToHex(b);

    return hex;
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
                        fl::string hexColor = rgbToHex(led[0], led[1], led[2]);
                        leds.push_back(fl::Json(hexColor));
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

// WLEDClient and FastLED adapter implementations (non-JSON, always available)

namespace fl {

// FastLEDAdapter implementation

FastLEDAdapter::FastLEDAdapter(uint8_t controllerIndex)
    : mControllerIndex(controllerIndex)
    , mSegmentStart(0)
    , mSegmentEnd(0)
    , mHasSegment(false)
{
    // Initialize segment end to the number of LEDs in the controller
    CLEDController& controller = FastLED[mControllerIndex];
    mSegmentEnd = controller.size();
}

fl::span<CRGB> FastLEDAdapter::getLEDs() {
    CLEDController& controller = FastLED[mControllerIndex];
    CRGB* leds = controller.leds();
    if (!leds) {
        return fl::span<CRGB>();
    }

    if (mHasSegment) {
        return fl::span<CRGB>(leds + mSegmentStart, mSegmentEnd - mSegmentStart);
    }
    return fl::span<CRGB>(leds, controller.size());
}

size_t FastLEDAdapter::getNumLEDs() const {
    if (mHasSegment) {
        return mSegmentEnd - mSegmentStart;
    }

    CLEDController& controller = FastLED[mControllerIndex];
    return controller.size();
}

void FastLEDAdapter::show() {
    FastLED.show();
}

void FastLEDAdapter::show(uint8_t brightness) {
    FastLED.show(brightness);
}

void FastLEDAdapter::clear(bool writeToStrip) {
    CLEDController& controller = FastLED[mControllerIndex];
    CRGB* leds = controller.leds();
    if (!leds) {
        return;
    }

    if (mHasSegment) {
        // Clear only the segment
        for (size_t i = mSegmentStart; i < mSegmentEnd; i++) {
            leds[i] = CRGB::Black;
        }
    } else {
        // Clear all LEDs
        size_t numLeds = controller.size();
        for (size_t i = 0; i < numLeds; i++) {
            leds[i] = CRGB::Black;
        }
    }

    if (writeToStrip) {
        FastLED.show();
    }
}

void FastLEDAdapter::setBrightness(uint8_t brightness) {
    FastLED.setBrightness(brightness);
}

uint8_t FastLEDAdapter::getBrightness() const {
    return FastLED.getBrightness();
}

void FastLEDAdapter::setCorrection(CRGB correction) {
    FastLED.setCorrection(correction);
}

void FastLEDAdapter::setTemperature(CRGB temperature) {
    FastLED.setTemperature(temperature);
}

void FastLEDAdapter::delay(unsigned long ms) {
    FastLED.delay(ms);
}

void FastLEDAdapter::setMaxRefreshRate(uint16_t fps) {
    FastLED.setMaxRefreshRate(fps);
}

uint16_t FastLEDAdapter::getMaxRefreshRate() const {
    // CFastLED doesn't expose the max refresh rate setting
    // Return 0 to indicate no limit
    return 0;
}

void FastLEDAdapter::setSegment(size_t start, size_t end) {
    CLEDController& controller = FastLED[mControllerIndex];
    size_t numLeds = controller.size();

    // Validate bounds
    if (start >= numLeds) {
        start = numLeds > 0 ? numLeds - 1 : 0;
    }
    if (end > numLeds) {
        end = numLeds;
    }
    if (end <= start) {
        end = start + 1;
        if (end > numLeds) {
            end = numLeds;
            start = end > 0 ? end - 1 : 0;
        }
    }

    mSegmentStart = start;
    mSegmentEnd = end;
    mHasSegment = true;
}

void FastLEDAdapter::clearSegment() {
    CLEDController& controller = FastLED[mControllerIndex];
    mSegmentEnd = controller.size();
    mSegmentStart = 0;
    mHasSegment = false;
}

// Helper function implementation
fl::shared_ptr<IFastLED> createFastLEDController(uint8_t controllerIndex) {
    return fl::make_shared<FastLEDAdapter>(controllerIndex);
}

// WLEDClient implementation

WLEDClient::WLEDClient(fl::shared_ptr<IFastLED> controller)
    : mController(controller), mBrightness(255), mOn(false) {
    if (!mController) {
        FL_WARN("WLEDClient: constructed with null controller");
    }
}

void WLEDClient::setBrightness(uint8_t brightness) {
    mBrightness = brightness;
    FL_DBG("WLEDClient: setBrightness(" << static_cast<int>(mBrightness) << ")");

    // Apply brightness to controller if we're on
    if (mOn && mController) {
        mController->setBrightness(mBrightness);
    }
}

void WLEDClient::setOn(bool on) {
    mOn = on;
    FL_DBG("WLEDClient: setOn(" << (mOn ? "true" : "false") << ")");

    if (!mController) {
        return;
    }

    if (mOn) {
        // Turning on: apply current brightness
        mController->setBrightness(mBrightness);
    } else {
        // Turning off: set brightness to 0 (but preserve internal brightness)
        mController->setBrightness(0);
    }
}

void WLEDClient::clear(bool writeToStrip) {
    FL_DBG("WLEDClient: clear(writeToStrip=" << (writeToStrip ? "true" : "false") << ")");

    if (!mController) {
        return;
    }

    mController->clear(writeToStrip);
}

void WLEDClient::update() {
    FL_DBG("WLEDClient: update()");

    if (!mController) {
        return;
    }

    mController->show();
}

fl::span<CRGB> WLEDClient::getLEDs() {
    if (!mController) {
        return fl::span<CRGB>();
    }

    return mController->getLEDs();
}

size_t WLEDClient::getNumLEDs() const {
    if (!mController) {
        return 0;
    }

    return mController->getNumLEDs();
}

void WLEDClient::setSegment(size_t start, size_t end) {
    FL_DBG("WLEDClient: setSegment(" << start << ", " << end << ")");

    if (!mController) {
        return;
    }

    mController->setSegment(start, end);
}

void WLEDClient::clearSegment() {
    FL_DBG("WLEDClient: clearSegment()");

    if (!mController) {
        return;
    }

    mController->clearSegment();
}

void WLEDClient::setCorrection(CRGB correction) {
    FL_DBG("WLEDClient: setCorrection(r=" << static_cast<int>(correction.r)
           << ", g=" << static_cast<int>(correction.g)
           << ", b=" << static_cast<int>(correction.b) << ")");

    if (!mController) {
        return;
    }

    mController->setCorrection(correction);
}

void WLEDClient::setTemperature(CRGB temperature) {
    FL_DBG("WLEDClient: setTemperature(r=" << static_cast<int>(temperature.r)
           << ", g=" << static_cast<int>(temperature.g)
           << ", b=" << static_cast<int>(temperature.b) << ")");

    if (!mController) {
        return;
    }

    mController->setTemperature(temperature);
}

void WLEDClient::setMaxRefreshRate(uint16_t fps) {
    FL_DBG("WLEDClient: setMaxRefreshRate(" << fps << ")");

    if (!mController) {
        return;
    }

    mController->setMaxRefreshRate(fps);
}

uint16_t WLEDClient::getMaxRefreshRate() const {
    if (!mController) {
        return 0;
    }

    return mController->getMaxRefreshRate();
}

} // namespace fl
