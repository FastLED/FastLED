#pragma once

#include "fl/stl/string.h"
#include "fl/json.h"
#include "fl/fx/wled/segment.h"

namespace fl {
namespace wled {

/**
 * @brief Parse hex color string to RGB components
 * @param hexStr Hex color string (e.g., "FF0000" or "#FF0000")
 * @param r Output: red component (0-255)
 * @param g Output: green component (0-255)
 * @param b Output: blue component (0-255)
 * @return true if parsing succeeded, false otherwise
 *
 * Supports formats:
 * - "RRGGBB" (6 hex digits)
 * - "#RRGGBB" (# prefix optional)
 */
bool parseHexColor(const fl::string& hexStr, uint8_t& r, uint8_t& g, uint8_t& b);

/**
 * @brief Convert RGB components to hex string
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @return Hex color string (format: "RRGGBB")
 */
fl::string rgbToHex(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Parse all fields from a segment JSON object into a WLEDSegment
 * @param segJson JSON object containing segment fields
 * @param seg Output: WLEDSegment to populate with parsed data
 *
 * Parses all WLED segment properties including:
 * - Position (start, stop, len)
 * - State (on, brightness)
 * - Colors (col array, individual LEDs)
 * - Effects (fx, sx, ix, pal, c1, c2, c3)
 * - Layout (grp, spc, of)
 * - Flags (sel, rev, mi, o1, o2, o3, rpt)
 * - Other (cct, si, m12, name)
 */
void parseSegmentFields(const fl::Json& segJson, WLEDSegment& seg);

} // namespace wled
} // namespace fl
