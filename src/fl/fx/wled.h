#pragma once

#include "fl/remote.h"
#include "fl/stl/vector.h"
#include "fl/stl/span.h"
#include "fl/stl/memory.h"
#include "fl/stl/shared_ptr.h"

// WLED subdirectory components (always available)
#include "fl/fx/wled/ifastled.h"
#include "fl/fx/wled/adapter.h"
#include "fl/fx/wled/client.h"

// WLED-specific Remote extension requires JSON support
#if FASTLED_ENABLE_JSON

#include "fl/fx/wled/segment.h"
#include "fl/fx/wled/json_helpers.h"

namespace fl {

/**
 * @brief WLED-specific Remote RPC extension
 *
 * Extends the base Remote RPC system with WLED state management
 * for controlling on/off state, brightness, segments, and effects.
 *
 * Provides methods to:
 * - Set WLED state from JSON objects (on/bri/segments/effects/playlists)
 * - Get current WLED state as JSON
 * - Query individual state fields (on/off, brightness, segments, etc.)
 *
 * ============================================================================
 * WLED JSON API REFERENCE (https://kno.wled.ge/interfaces/json-api/)
 * ============================================================================
 *
 * ABOUT WLED:
 * -----------
 * WLED is a popular ESP8266/ESP32 firmware for controlling addressable LEDs
 * over WiFi. It provides a comprehensive JSON API via HTTP endpoints like
 * /json, /json/state, /json/info, /json/eff, /json/pal, etc.
 *
 * FastLED does NOT implement a network server or HTTP endpoints. This
 * reference documents WLED's JSON data structures and control properties
 * that could be used for interoperability or implementing WLED-compatible
 * control interfaces in FastLED-based projects
 *
 * STATE OBJECT PROPERTIES:
 * ------------------------
 * Property       | Range         | Description
 * ---------------|---------------|----------------------------------------------
 * on             | bool or "t"   | Power toggle (true=on, false=off, "t"=toggle)
 * bri            | 0-255         | Master brightness level
 * transition     | 0-65535       | Crossfade duration (×100ms, e.g., 7 = 700ms)
 * ps             | -1 to 250     | Active preset ID (-1=none)
 * pl             | -1 to 250     | Active playlist ID (-1=none)
 * nl             | object        | Nightlight configuration (see below)
 * udpn           | object        | UDP sync settings
 * lor            | 0-2           | Live data override (0=off, 1=override, 2=until reboot)
 * mainseg        | 0-maxseg-1    | Main segment for global controls
 * seg            | array         | Segment configurations (see below)
 * playlist       | object        | Playlist configuration (see below)
 *
 * SEGMENT CONTROL (seg array):
 * -----------------------------
 * Property       | Range         | Description
 * ---------------|---------------|----------------------------------------------
 * id             | 0-maxseg-1    | Segment identifier (required for updates)
 * start          | LED index     | Starting LED position (inclusive)
 * stop           | LED index     | Ending LED position (exclusive)
 * len            | count         | Segment length (alternative to stop)
 * grp            | count         | LED grouping factor
 * spc            | count         | Spacing between groups
 * of             | offset        | Group offset
 * on             | bool or "t"   | Segment power state
 * bri            | 0-255         | Segment brightness
 * cct            | 0-255 or K    | Color temperature (Kelvin 1900-10091 or 0-255)
 * col            | array         | Color slots [[R,G,B(,W)], ...] or ["RRGGBB", ...]
 *                |               | - Index 0: Primary color
 *                |               | - Index 1: Secondary color
 *                |               | - Index 2: Tertiary color
 * fx             | 0-fxcount-1   | Effect ID
 * sx             | 0-255         | Effect speed
 * ix             | 0-255         | Effect intensity
 * pal            | 0-palcount-1  | Palette ID
 * c1, c2, c3     | 0-255         | Effect custom parameters
 * sel            | bool          | Segment selected for API control
 * rev            | bool          | Reverse segment direction
 * mi             | bool          | Mirror effect within segment
 * o1, o2, o3     | bool          | Effect option flags
 * si             | 0-3           | Sound simulation mode
 * m12            | 0-3           | Segment mapping mode
 * i              | array         | Individual LED control: ["RRGGBB", "RRGGBB", ...]
 *                |               | - Use LED index or range: "i":["FF0000|10","00FF00|20-30"]
 * n              | string        | Segment name
 * rpt            | bool          | Repeat segment pattern
 *
 * COLOR CONTROL:
 * --------------
 * Colors: RGB(W) arrays [[R,G,B(,W)], ...] or hex strings ["RRGGBB", ...]
 * Example: {"seg":[{"col":[[255,170,0],[0,0,0],[64,64,64]]}]}
 *       or {"seg":[{"col":["FFAA00","000000","404040"]}]}
 *
 * Individual LEDs: {"seg":{"i":["FF0000","00FF00","0000FF"]}}
 * - Sets first 3 LEDs to red, green, blue
 * - Range syntax: "i":["FF0000|10-20"] sets LEDs 10-20 to red
 *
 * INFO OBJECT (read-only):
 * ------------------------
 * Property              | Description
 * ----------------------|-----------------------------------------------
 * ver                   | WLED version string
 * vid                   | Version ID (integer)
 * leds.count            | Total LED count
 * leds.rgbw             | True if 4-channel (RGBW) support
 * leds.wv               | True if WLED version supports white channel
 * leds.cct              | True if CCT control available
 * leds.pwr              | Current power consumption (mW)
 * leds.maxpwr           | Configured max power limit (mW)
 * leds.seglock          | True if segments locked
 * str                   | Info string
 * name                  | Device name
 * udpport               | UDP port number
 * live                  | True if in live/realtime mode
 * lm                    | Live mode data source
 * lip                   | Live data source IP
 * ws                    | WebSocket client count
 * fxcount               | Number of available effects
 * palcount              | Number of available palettes
 * cpalcount             | Number of custom palettes
 * maps                  | LED mapping count
 * wifi.bssid            | Connected AP BSSID
 * wifi.signal           | WiFi signal strength (0-100)
 * wifi.channel          | WiFi channel
 * fs.u                  | Used filesystem space (KB)
 * fs.t                  | Total filesystem space (KB)
 * fs.pmt                | Last modification time
 * ndc                   | Node discovery count
 * arch                  | Architecture (esp8266/esp32/etc.)
 * core                  | Core/SDK version
 * lwip                  | LwIP version
 * freeheap              | Free heap memory (bytes)
 * uptime                | Uptime in seconds
 * opt                   | Build options/features bitfield
 * brand                 | Brand identifier
 * product               | Product identifier
 * mac                   | MAC address
 * ip                    | IP address
 *
 * NIGHTLIGHT (nl object):
 * -----------------------
 * Property       | Range         | Description
 * ---------------|---------------|----------------------------------------------
 * on             | bool          | Nightlight active
 * dur            | 1-255         | Duration in minutes
 * mode           | 0-3           | Transition mode:
 *                |               | 0=Instant off, 1=Fade to off, 2=Color fade,
 *                |               | 3=Sunrise (warmth increase)
 * tbri           | 0-255         | Target brightness for nightlight end
 * rem            | -1 to 255     | Remaining nightlight duration (read-only)
 *
 * Example: {"nl":{"on":true,"dur":10,"mode":1}}
 *
 * PLAYLIST (playlist object):
 * ---------------------------
 * Property       | Description
 * ---------------|----------------------------------------------------------
 * ps             | Array of preset IDs to cycle through
 * dur            | Array of durations (seconds) for each preset
 * transition     | Array of transition times (×100ms) for each preset
 * repeat         | Number of cycles (0=infinite)
 * end            | End preset ID (after playlist completes)
 * r              | True for randomized playback order
 *
 * Example: {"playlist":{"ps":[26,20,18],"dur":[30,20,10],"repeat":10}}
 *
 * EFFECT METADATA (/json/fxdata):
 * --------------------------------
 * Format: "<parameters>;<colors>;<palette>;<flags>;<defaults>"
 * - Parameters: Control sliders/inputs for the effect
 * - Colors: Number of color slots used (0-3)
 * - Palette: True if effect uses palette
 * - Flags: Effect characteristics:
 *   - '1' = Optimized for 1D strips
 *   - '2' = Requires 2D matrix
 *   - 'v' = Volume reactive (audio)
 *   - 'f' = Frequency reactive (audio)
 * - Defaults: Default values for parameters
 *
 * CCT (COLOR TEMPERATURE):
 * ------------------------
 * - Accepts relative values (0-255) or Kelvin (1900-10091)
 * - Only affects output when:
 *   - White balance correction enabled, OR
 *   - CCT-capable bus configured (dual white LEDs)
 * - Example: {"seg":[{"cct":127}]} or {"seg":[{"cct":3000}]}
 *
 * WLED JSON CONTROL EXAMPLES (for reference):
 * --------------------------------------------
 * 1. Turn on at full brightness:
 *    {"on":true,"bri":255}
 *
 * 2. Set segment 0 to teal color:
 *    {"seg":[{"id":0,"col":[[0,255,200]]}]}
 *
 * 3. Toggle segment 2:
 *    {"seg":[{"id":2,"on":"t"}]}
 *
 * 4. Apply effect 5 to segment 0 at speed 128, intensity 200:
 *    {"seg":[{"id":0,"fx":5,"sx":128,"ix":200}]}
 *
 * 5. Set custom LED pattern (first 3 LEDs):
 *    {"seg":{"i":["FF0000","00FF00","0000FF"]}}
 *
 * 6. Enable nightlight (fade to off in 30 minutes):
 *    {"nl":{"on":true,"dur":30,"mode":1}}
 *
 * 7. Load preset 5:
 *    {"ps":5}
 *
 * 8. Start playlist cycling through presets:
 *    {"playlist":{"ps":[1,2,3],"dur":[20,20,20],"repeat":0}}
 *
 * WLED INTEGRATION NOTES:
 * ------------------------
 * - WLED uses JSON for state management (not HTTP API for new integrations)
 * - Partial updates supported: only changed properties need to be sent
 * - Segments allow independent control of different LED strip regions
 * - Effects and palettes are device-specific in WLED
 * - WLED supports UDP sync for multi-device synchronization
 * - Presets store complete state snapshots for quick recall
 * - Playlists automate preset sequences with timing control
 *
 * FASTLED IMPLEMENTATION:
 * -----------------------
 * This class currently implements basic WLED state management (on/bri).
 * Future enhancements could include:
 * - Segment control (multiple independent LED regions)
 * - Effect parameter mapping (fx, sx, ix, pal)
 * - Nightlight timer functionality
 * - Preset/playlist support
 * - Color temperature (CCT) control
 * - Individual LED control syntax parsing
 *
 * ============================================================================
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
     *   remote.setState(state);
     */
    void setState(const fl::Json& wledState);

    /**
     * @brief Get current WLED state as JSON object
     * @return JSON object with "on" and "bri" fields
     *
     * Example:
     *   fl::Json state = remote.getState();
     *   // Returns: {"on":true,"bri":128}
     */
    fl::Json getState() const;

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

    /**
     * @brief Get transition duration
     * @return Crossfade duration in units of 100ms (0-65535)
     */
    uint16_t getTransition() const { return mTransition; }

    /**
     * @brief Get active preset ID
     * @return Preset ID (-1 = none, 0-250)
     */
    int16_t getPreset() const { return mPreset; }

    /**
     * @brief Get active playlist ID
     * @return Playlist ID (-1 = none, 0-250)
     */
    int16_t getPlaylist() const { return mPlaylist; }

    /**
     * @brief Get live data override setting
     * @return Live override (0=off, 1=override, 2=until reboot)
     */
    uint8_t getLiveOverride() const { return mLiveOverride; }

    /**
     * @brief Get main segment index
     * @return Main segment ID for global controls
     */
    uint8_t getMainSegment() const { return mMainSegment; }

    /**
     * @brief Get nightlight active state
     * @return true if nightlight is active
     */
    bool getNightlightOn() const { return mNightlightOn; }

    /**
     * @brief Get nightlight duration
     * @return Duration in minutes (1-255)
     */
    uint8_t getNightlightDuration() const { return mNightlightDuration; }

    /**
     * @brief Get nightlight mode
     * @return Mode (0=instant off, 1=fade, 2=color fade, 3=sunrise)
     */
    uint8_t getNightlightMode() const { return mNightlightMode; }

    /**
     * @brief Get nightlight target brightness
     * @return Target brightness (0-255)
     */
    uint8_t getNightlightTargetBrightness() const { return mNightlightTargetBrightness; }

    /**
     * @brief Get playlist configuration
     * @return JSON object with playlist settings (ps, dur, transition, repeat, end, r)
     */
    fl::Json getPlaylistConfig() const;

    /**
     * @brief Check if playlist is active
     * @return true if playlist has preset IDs configured
     */
    bool hasPlaylistConfig() const { return !mPlaylistPresets.empty(); }

    /**
     * @brief Get UDP sync send setting
     * @return true if UDP broadcast packets should be sent on state change
     */
    bool getUdpSend() const { return mUdpSend; }

    /**
     * @brief Get UDP sync receive setting
     * @return true if UDP broadcast packets should be received
     */
    bool getUdpReceive() const { return mUdpReceive; }

    /**
     * @brief Get segment count
     * @return Number of configured segments
     */
    size_t getSegmentCount() const { return mSegments.size(); }

    /**
     * @brief Get segment by index
     * @param index Segment index in the array (not segment ID)
     * @return Reference to the segment
     */
    const WLEDSegment& getSegment(size_t index) const { return mSegments[index]; }

    /**
     * @brief Get all segments
     * @return Const reference to the segments vector
     */
    const fl::vector<WLEDSegment>& getSegments() const { return mSegments; }

    /**
     * @brief Find segment by ID
     * @param id Segment ID to find
     * @return Pointer to segment if found, nullptr otherwise
     */
    const WLEDSegment* findSegmentById(uint8_t id) const;

private:
    // WLED state (runtime-only, no persistence)
    bool mWledOn = false;           // WLED on/off state
    uint8_t mWledBri = 255;         // WLED brightness (0-255)
    uint16_t mTransition = 7;       // Crossfade duration (×100ms, default 700ms)
    int16_t mPreset = -1;           // Active preset ID (-1 = none)
    int16_t mPlaylist = -1;         // Active playlist ID (-1 = none)
    uint8_t mLiveOverride = 0;      // Live data override (0=off, 1=override, 2=until reboot)
    uint8_t mMainSegment = 0;       // Main segment for global controls

    // Nightlight state
    bool mNightlightOn = false;          // Nightlight active
    uint8_t mNightlightDuration = 60;    // Nightlight duration in minutes (default 60)
    uint8_t mNightlightMode = 1;         // Nightlight mode (0=instant, 1=fade, 2=color, 3=sunrise)
    uint8_t mNightlightTargetBrightness = 0;  // Target brightness for nightlight end

    // Playlist configuration
    fl::vector<int16_t> mPlaylistPresets;      // Array of preset IDs to cycle through
    fl::vector<uint16_t> mPlaylistDurations;   // Array of durations (seconds) for each preset
    fl::vector<uint16_t> mPlaylistTransitions; // Array of transition times (×100ms) for each preset
    uint16_t mPlaylistRepeat = 0;              // Number of cycles (0=infinite)
    int16_t mPlaylistEnd = -1;                 // End preset ID (after playlist completes, -1=none)
    bool mPlaylistRandomize = false;           // True for randomized playback order

    // UDP sync settings
    bool mUdpSend = false;    // Send UDP broadcast packets on state change
    bool mUdpReceive = true;  // Receive UDP broadcast packets

    // Segment configurations
    fl::vector<WLEDSegment> mSegments;  // Array of segment configurations
};

} // namespace fl

#endif // FASTLED_ENABLE_JSON
