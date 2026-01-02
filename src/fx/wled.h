#pragma once

#include "fl/remote.h"
#include "fl/stl/vector.h"
#include "fl/stl/span.h"
#include "fl/stl/memory.h"
#include "fl/stl/shared_ptr.h"

// WLED-specific Remote extension requires JSON support
#if FASTLED_ENABLE_JSON

namespace fl {

/**
 * @brief WLED segment configuration
 *
 * Represents a single segment with independent control of LEDs.
 * Segments allow multiple regions of an LED strip to have different
 * colors, brightness, and effects.
 */
struct WLEDSegment {
    uint8_t mId = 0;              // Segment ID (0-255)
    uint16_t mStart = 0;          // Starting LED index (inclusive)
    uint16_t mStop = 0;           // Ending LED index (exclusive)
    bool mOn = true;              // Segment power state
    uint8_t mBri = 255;           // Segment brightness (0-255)

    // Color slots: each color is [R,G,B] or [R,G,B,W]
    // Index 0: Primary color, Index 1: Secondary color, Index 2: Tertiary color
    fl::vector<fl::vector<uint8_t>> mColors;

    // Layout properties
    uint16_t mLen = 0;            // Segment length (alternative to stop, 0=use stop)
    uint8_t mGrp = 1;             // LED grouping factor (default 1)
    uint8_t mSpc = 0;             // Spacing between groups (default 0)
    uint16_t mOf = 0;             // Group offset (default 0)

    // Effect properties
    uint8_t mFx = 0;              // Effect ID (0-fxcount-1)
    uint8_t mSx = 128;            // Effect speed (0-255, default 128)
    uint8_t mIx = 128;            // Effect intensity (0-255, default 128)
    uint8_t mPal = 0;             // Palette ID (0-palcount-1)
    uint8_t mC1 = 128;            // Effect custom parameter 1 (0-255)
    uint8_t mC2 = 128;            // Effect custom parameter 2 (0-255)
    uint8_t mC3 = 16;             // Effect custom parameter 3 (0-255)

    // Effect option flags
    bool mSel = false;            // Segment selected for API control
    bool mRev = false;            // Reverse segment direction
    bool mMi = false;             // Mirror effect within segment
    bool mO1 = false;             // Effect option flag 1
    bool mO2 = false;             // Effect option flag 2
    bool mO3 = false;             // Effect option flag 3

    // Other properties
    uint16_t mCct = 0;            // Color temperature (0-255 or Kelvin 1900-10091, 0=not set)
    uint8_t mSi = 0;              // Sound simulation mode (0-3)
    uint8_t mM12 = 0;             // Segment mapping mode (0-3)
    bool mRpt = false;            // Repeat segment pattern
    fl::string mName;             // Segment name (optional)

    // Individual LED control: stores per-LED colors as RGB triplets
    // Each inner vector is [R,G,B] for a specific LED
    fl::vector<fl::vector<uint8_t>> mIndividualLeds;
};

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

// WLEDClient and FastLED adapter (non-JSON, always available)

namespace fl {

/**
 * @brief Pure virtual interface for FastLED operations
 *
 * This interface abstracts FastLED operations to enable dependency injection
 * and testing. It represents the core LED control operations that WLED and
 * other integrations need to perform.
 *
 * This interface allows:
 * - Real implementations that delegate to the global FastLED singleton
 * - Mock implementations for unit testing without hardware
 * - Custom implementations for specialized control scenarios
 *
 * @see FastLEDAdapter for the real implementation
 * @see MockFastLEDController for the test mock implementation
 */
class IFastLED {
public:
    virtual ~IFastLED() = default;

    // LED array access

    /**
     * @brief Get the LED array as a span
     * @return Span over the LED array (may be full array or current segment)
     */
    virtual fl::span<CRGB> getLEDs() = 0;

    /**
     * @brief Get the number of LEDs
     * @return Number of LEDs in the current context (full array or segment)
     */
    virtual size_t getNumLEDs() const = 0;

    // Output control

    /**
     * @brief Send the LED data to the strip
     * Uses the current brightness setting
     */
    virtual void show() = 0;

    /**
     * @brief Send the LED data to the strip with a specific brightness
     * @param brightness Brightness level (0-255)
     */
    virtual void show(uint8_t brightness) = 0;

    /**
     * @brief Clear all LEDs (set to black)
     * @param writeToStrip If true, immediately write to strip (call show())
     */
    virtual void clear(bool writeToStrip = false) = 0;

    // Brightness

    /**
     * @brief Set the global brightness
     * @param brightness Brightness level (0-255)
     */
    virtual void setBrightness(uint8_t brightness) = 0;

    /**
     * @brief Get the current global brightness
     * @return Brightness level (0-255)
     */
    virtual uint8_t getBrightness() const = 0;

    // Color correction

    /**
     * @brief Set color correction
     * @param correction Color correction RGB values
     */
    virtual void setCorrection(CRGB correction) = 0;

    /**
     * @brief Set color temperature
     * @param temperature Color temperature RGB values
     */
    virtual void setTemperature(CRGB temperature) = 0;

    // Timing

    /**
     * @brief Delay for a specified number of milliseconds
     * @param ms Delay duration in milliseconds
     */
    virtual void delay(unsigned long ms) = 0;

    /**
     * @brief Set the maximum refresh rate
     * @param fps Maximum frames per second (0 = no limit)
     */
    virtual void setMaxRefreshRate(uint16_t fps) = 0;

    /**
     * @brief Get the maximum refresh rate
     * @return Maximum frames per second (0 = no limit)
     */
    virtual uint16_t getMaxRefreshRate() const = 0;

    // Advanced features (segment support for WLED)

    /**
     * @brief Set a segment range for subsequent operations
     * @param start Starting LED index (inclusive)
     * @param end Ending LED index (exclusive)
     *
     * After calling this, getLEDs() and getNumLEDs() will operate on the
     * specified segment only.
     */
    virtual void setSegment(size_t start, size_t end) = 0;

    /**
     * @brief Clear the segment range (operate on full LED array)
     */
    virtual void clearSegment() = 0;
};

/**
 * @brief Real FastLED implementation adapter
 *
 * This class adapts the global FastLED singleton to the IFastLED interface.
 * It provides a thin wrapper that delegates all operations to the global
 * FastLED object while optionally managing segment control for specific
 * LED controller indices.
 *
 * This adapter allows WLED and other integrations to work with FastLED through
 * a standard interface while maintaining full compatibility with the existing
 * FastLED API.
 *
 * Example usage:
 * @code
 * CRGB leds[100];
 * FastLED.addLeds<WS2812, 5>(leds, 100);
 *
 * auto controller = fl::make_shared<FastLEDAdapter>();
 * controller->setBrightness(128);
 * controller->show();
 * @endcode
 *
 * @see IFastLED for the interface definition
 * @see createFastLEDController() for helper functions to create adapters
 */
class FastLEDAdapter : public IFastLED {
public:
    /**
     * @brief Construct adapter wrapping the global FastLED object
     * @param controllerIndex Index of the LED controller to use (default: 0)
     *                        Use 0 for the first controller, 1 for second, etc.
     */
    explicit FastLEDAdapter(uint8_t controllerIndex = 0);

    /**
     * @brief Destructor
     */
    ~FastLEDAdapter() override = default;

    // IFastLED interface implementation

    fl::span<CRGB> getLEDs() override;
    size_t getNumLEDs() const override;

    void show() override;
    void show(uint8_t brightness) override;
    void clear(bool writeToStrip = false) override;

    void setBrightness(uint8_t brightness) override;
    uint8_t getBrightness() const override;

    void setCorrection(CRGB correction) override;
    void setTemperature(CRGB temperature) override;

    void delay(unsigned long ms) override;
    void setMaxRefreshRate(uint16_t fps) override;
    uint16_t getMaxRefreshRate() const override;

    void setSegment(size_t start, size_t end) override;
    void clearSegment() override;

private:
    uint8_t mControllerIndex; // Index of the LED controller in FastLED
    size_t mSegmentStart;     // Start of current segment (0 if no segment)
    size_t mSegmentEnd;       // End of current segment (numLeds if no segment)
    bool mHasSegment;         // True if a segment is active
};

/**
 * @brief Create a FastLED controller adapter
 * @param controllerIndex Index of the LED controller to use (default: 0)
 * @return Shared pointer to the adapter
 *
 * Example:
 * @code
 * CRGB leds[100];
 * FastLED.addLeds<WS2812, 5>(leds, 100);
 * auto controller = createFastLEDController();  // Uses controller index 0
 * @endcode
 */
fl::shared_ptr<IFastLED> createFastLEDController(uint8_t controllerIndex = 0);

/**
 * @brief WLED Client for controlling LEDs through FastLED interface
 *
 * Provides a simplified interface for controlling LEDs with WLED-style
 * operations (brightness, on/off, clear). Uses dependency injection to
 * allow both real FastLED control and mock implementations for testing.
 *
 * Example usage:
 *   // Real usage
 *   auto controller = createFastLEDController(leds, NUM_LEDS);
 *   WLEDClient client(controller);
 *   client.setBrightness(128);
 *   client.setOn(true);
 *
 *   // Test usage
 *   auto mock = fl::make_shared<MockFastLEDController>(100);
 *   WLEDClient client(mock);
 *   client.setBrightness(128);
 *   REQUIRE(mock->getLastBrightness() == 128);
 */
class WLEDClient {
public:
    /**
     * @brief Construct WLEDClient with FastLED controller
     * @param controller Shared pointer to IFastLED implementation
     */
    explicit WLEDClient(fl::shared_ptr<IFastLED> controller);

    /**
     * @brief Set brightness level
     * @param brightness Brightness value (0-255)
     *
     * Updates the brightness and applies to controller if client is on.
     */
    void setBrightness(uint8_t brightness);

    /**
     * @brief Get current brightness level
     * @return Brightness value (0-255)
     */
    uint8_t getBrightness() const { return mBrightness; }

    /**
     * @brief Set on/off state
     * @param on True to turn on, false to turn off
     *
     * When turning on, applies current brightness to controller.
     * When turning off, sets controller brightness to 0 but preserves internal brightness.
     */
    void setOn(bool on);

    /**
     * @brief Get on/off state
     * @return True if on, false if off
     */
    bool getOn() const { return mOn; }

    /**
     * @brief Clear all LEDs
     * @param writeToStrip If true, immediately writes to strip (calls show)
     *
     * Sets all LEDs to black/off. If writeToStrip is true, also calls
     * controller->show() to update the physical strip.
     */
    void clear(bool writeToStrip = false);

    /**
     * @brief Update physical LED strip
     *
     * Calls controller->show() to write LED data to the physical strip.
     * Typically called after making changes to LED colors.
     */
    void update();

    /**
     * @brief Get access to LED array
     * @return Span view of LED array
     *
     * Allows direct manipulation of LED colors through the controller.
     */
    fl::span<CRGB> getLEDs();

    /**
     * @brief Get number of LEDs
     * @return LED count
     */
    size_t getNumLEDs() const;

    /**
     * @brief Set a segment range for subsequent operations
     * @param start Starting LED index (inclusive)
     * @param end Ending LED index (exclusive)
     *
     * After calling this, operations will affect only the specified segment.
     * Delegates to controller's setSegment method.
     */
    void setSegment(size_t start, size_t end);

    /**
     * @brief Clear the segment range (operate on full LED array)
     *
     * Restores operations to affect the entire LED array.
     * Delegates to controller's clearSegment method.
     */
    void clearSegment();

    /**
     * @brief Set color correction
     * @param correction Color correction RGB values
     *
     * Adjusts color output to compensate for LED characteristics.
     * Delegates to controller's setCorrection method.
     */
    void setCorrection(CRGB correction);

    /**
     * @brief Set color temperature
     * @param temperature Color temperature RGB values
     *
     * Adjusts color output for perceived color temperature.
     * Delegates to controller's setTemperature method.
     */
    void setTemperature(CRGB temperature);

    /**
     * @brief Set maximum refresh rate
     * @param fps Maximum frames per second (0 = no limit)
     *
     * Limits how often the LED strip can be updated.
     * Delegates to controller's setMaxRefreshRate method.
     */
    void setMaxRefreshRate(uint16_t fps);

    /**
     * @brief Get maximum refresh rate
     * @return Maximum frames per second (0 = no limit)
     */
    uint16_t getMaxRefreshRate() const;

private:
    fl::shared_ptr<IFastLED> mController;  // FastLED controller interface
    uint8_t mBrightness;                    // Current brightness (0-255)
    bool mOn;                                // On/off state
};

} // namespace fl
