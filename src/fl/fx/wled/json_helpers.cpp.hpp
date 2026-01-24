#include "fl/fx/wled/json_helpers.h"
#include "fl/warn.h"
#include "fl/stl/cstdlib.h"  // For strtol

namespace fl {
namespace wled {

// Helper function to parse hex color string to RGB
bool parseHexColor(const fl::string& hexStr, uint8_t& r, uint8_t& g, uint8_t& b) {
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

// Helper function to convert RGB to hex string
fl::string rgbToHex(uint8_t r, uint8_t g, uint8_t b) {
    fl::string hex;
    hex.reserve(6);

    auto byteToHex = [](uint8_t val) -> fl::string {
        const char hexChars[] = "0123456789ABCDEF";
        fl::string result;
        result.push_back(hexChars[val >> 4]);
        result.push_back(hexChars[val & 0x0F]);
        return result;
    };

    hex += byteToHex(r);
    hex += byteToHex(g);
    hex += byteToHex(b);

    return hex;
}

// Helper function to parse segment fields
void parseSegmentFields(const fl::Json& segJson, WLEDSegment& seg) {
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
                    long startVal = fl::strtol(startStr.c_str(), &endptr, 10);
                    if (endptr == startStr.c_str() || startVal < 0) {
                        FL_WARN("WLED: invalid range start index: " << startStr);
                        continue;
                    }

                    long endVal = fl::strtol(endStr.c_str(), &endptr, 10);
                    if (endptr == endStr.c_str() || endVal < 0) {
                        FL_WARN("WLED: invalid range end index: " << endStr);
                        continue;
                    }

                    startIdx = static_cast<size_t>(startVal);
                    endIdx = static_cast<size_t>(endVal);
                } else {
                    // Single index format
                    char* endptr;
                    long idxVal = fl::strtol(indexStr.c_str(), &endptr, 10);
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

} // namespace wled
} // namespace fl
