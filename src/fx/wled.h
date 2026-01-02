#pragma once

#include "fl/remote.h"

// WLED-specific Remote extension requires JSON support
#if FASTLED_ENABLE_JSON

namespace fl {

/**
 * @brief WLED-specific Remote RPC extension
 *
 * Extends the base Remote RPC system with WLED state management
 * for controlling on/off state and brightness.
 *
 * Provides methods to:
 * - Set WLED state from JSON objects ("on" and "bri" fields)
 * - Get current WLED state as JSON
 * - Query individual state fields (on/off, brightness)
 */
class WLED : public Remote {
public:
    WLED() = default;

    /**
     * @brief Set WLED state from JSON object
     * @param wledState JSON object containing "on" (bool) and/or "bri" (0-255) fields
     *
     * Extracts WLED control fields and updates internal state:
     * - "on": boolean on/off state (optional, keeps existing value if missing)
     * - "bri": brightness 0-255 (optional, keeps existing value if missing)
     *
     * Example:
     *   fl::Json state = fl::Json::parse(R"({"on":true,"bri":128})");
     *   remote.setWledState(state);
     */
    void setWledState(const fl::Json& wledState);

    /**
     * @brief Get current WLED state as JSON object
     * @return JSON object with "on" and "bri" fields
     *
     * Example:
     *   fl::Json state = remote.getWledState();
     *   // Returns: {"on":true,"bri":128}
     */
    fl::Json getWledState() const;

    /**
     * @brief Get WLED on/off state
     * @return true if WLED is on, false if off
     */
    bool getOn() const { return mWledOn; }

    /**
     * @brief Get WLED brightness
     * @return Brightness value 0-255
     */
    uint8_t getBrightness() const { return mWledBri; }

private:
    // WLED state (runtime-only, no persistence)
    bool mWledOn = false;       // WLED on/off state
    uint8_t mWledBri = 255;     // WLED brightness (0-255)
};

} // namespace fl

#endif // FASTLED_ENABLE_JSON
