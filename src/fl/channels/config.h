#pragma once

#include "crgb.h"  // IWYU pragma: keep
#include "fl/gfx/rgbw.h"  // IWYU pragma: keep
#include "fl/stl/span.h"
#include "fl/stl/vector.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/variant.h"
#include "fl/math/screenmap.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/chipsets/clockless_encoder.h"
#include "fl/chipsets/spi.h"
#include "fl/gfx/eorder.h"
#include "fl/channels/options.h"
#include "fl/stl/optional.h"
#include "color.h"  // IWYU pragma: keep
#include "dither_mode.h"  // IWYU pragma: keep
#include "fl/stl/noexcept.h"

namespace fl {

/// @brief Clockless chipset configuration (single data pin)
///
/// Used for timing-sensitive LED protocols like WS2812, SK6812, APA106, etc.
/// These chipsets encode data using precise nanosecond timing on a single data line.
///
/// Carries three concerns as peer fields:
///   - `pin`     — GPIO data pin
///   - `timing`  — bit-period timing (T1/T2/T3/RESET)
///   - `encoder` — byte-level encoding pipeline (WS2812 vs UCS7604 variants)
struct ClocklessChipset {
    int pin;                        ///< GPIO data pin
    ChipsetTimingConfig timing;     ///< T1/T2/T3 timing parameters
    ClocklessEncoder encoder;       ///< Byte-level encoding pipeline (default: WS2812)

    /// @brief Constructor with explicit encoder
    constexpr ClocklessChipset(int pin, const ChipsetTimingConfig& timing,
                               ClocklessEncoder encoder) FL_NOEXCEPT
        : pin(pin), timing(timing), encoder(encoder) {}

    /// @brief Constructor (encoder defaults to WS2812)
    constexpr ClocklessChipset(int pin, const ChipsetTimingConfig& timing) FL_NOEXCEPT
        : pin(pin), timing(timing), encoder(ClocklessEncoder::CLOCKLESS_ENCODER_WS2812) {}

    /// @brief Default constructor
    constexpr ClocklessChipset() FL_NOEXCEPT
        : pin(-1), timing(0, 0, 0, 0), encoder(ClocklessEncoder::CLOCKLESS_ENCODER_WS2812) {}

    /// @brief Copy constructor
    ClocklessChipset(const ClocklessChipset&) FL_NOEXCEPT = default;

    /// @brief Move constructor
    ClocklessChipset(ClocklessChipset&&) FL_NOEXCEPT = default;

    /// @brief Copy assignment
    ClocklessChipset& operator=(const ClocklessChipset&) FL_NOEXCEPT = default;

    /// @brief Move assignment
    ClocklessChipset& operator=(ClocklessChipset&&) FL_NOEXCEPT = default;

    /// @brief Equality operator
    bool operator==(const ClocklessChipset& other) const FL_NOEXCEPT {
        return pin == other.pin &&
               timing == other.timing &&
               encoder == other.encoder;
    }

    /// @brief Inequality operator
    bool operator!=(const ClocklessChipset& other) const FL_NOEXCEPT {
        return !(*this == other);
    }
};

/// @brief Build a `ClocklessChipset` from a compile-time TIMING trait
///
/// Collapses the historical two-step pattern:
/// ```cpp
/// auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
/// fl::ClocklessChipset chipset(PIN, timing);
/// ```
/// into a single expression:
/// ```cpp
/// auto chipset = fl::makeClockless<fl::TIMING_WS2812_800KHZ>(PIN);
/// ```
///
/// The encoder selector is extracted from the TIMING trait via `encoder_for<>()`:
/// timings with a static `ENCODER` member (e.g., `TIMING_UCS7604_800KHZ`) yield
/// that encoder; others default to `CLOCKLESS_ENCODER_WS2812`.
///
/// @tparam TIMING Compile-time chipset timing trait (e.g., `TIMING_WS2812_800KHZ`)
/// @param  pin    GPIO data pin
template <typename TIMING>
constexpr ClocklessChipset makeClockless(int pin) FL_NOEXCEPT {
    return ClocklessChipset(pin, makeTimingConfig<TIMING>(), encoder_for<TIMING>());
}

/// @brief SPI chipset configuration (data + clock pins)
///
/// Used for clock-based LED protocols like APA102, SK9822, HD108, WS2801, etc.
/// These chipsets use explicit clock and data lines for synchronous transmission.
struct SpiChipsetConfig {
    int dataPin;                    ///< GPIO data pin (MOSI)
    int clockPin;                   ///< GPIO clock pin (SCK)
    SpiEncoder timing;              ///< SPI encoder configuration

    /// @brief Constructor
    SpiChipsetConfig(int dataPin, int clockPin, const SpiEncoder& timing) FL_NOEXCEPT
        : dataPin(dataPin), clockPin(clockPin), timing(timing) {}

    /// @brief Default constructor (requires explicit protocol specification)
    /// @note Protocol defaults to APA102 as a reasonable fallback
    SpiChipsetConfig() FL_NOEXCEPT : dataPin(-1), clockPin(-1), timing{fl::SpiChipset::APA102, 6000000} {}

    /// @brief Copy constructor
    SpiChipsetConfig(const SpiChipsetConfig&) FL_NOEXCEPT = default;

    /// @brief Move constructor
    SpiChipsetConfig(SpiChipsetConfig&&) FL_NOEXCEPT = default;

    /// @brief Copy assignment
    SpiChipsetConfig& operator=(const SpiChipsetConfig&) FL_NOEXCEPT = default;

    /// @brief Move assignment
    SpiChipsetConfig& operator=(SpiChipsetConfig&&) FL_NOEXCEPT = default;

    /// @brief Equality operator
    bool operator==(const SpiChipsetConfig& other) const FL_NOEXCEPT {
        return dataPin == other.dataPin &&
               clockPin == other.clockPin &&
               timing == other.timing;
    }

    /// @brief Inequality operator
    bool operator!=(const SpiChipsetConfig& other) const FL_NOEXCEPT {
        return !(*this == other);
    }
};

/// @brief Variant type that holds either a clockless or SPI chipset configuration
///
/// This allows ChannelConfig to support both chipset types with compile-time type safety
/// and runtime polymorphism via the visitor pattern.
///
/// @example
/// ```cpp
/// // Clockless chipset (WS2812)
/// ChipsetVariant clockless = ClocklessChipset(5, makeTimingConfig<TIMING_WS2812_800KHZ>());
///
/// // SPI chipset (APA102)
/// ChipsetVariant spi = SpiChipsetConfig(23, 18, SpiEncoder::apa102());
/// ```
using ChipsetVariant = fl::variant<ClocklessChipset, SpiChipsetConfig>;

/// @brief Configuration for a single LED channel
///
/// Contains all settings typically configured via FastLED.addLeds<>().set...() methods:
/// - LED data array and count
/// - Chipset configuration (clockless or SPI)
/// - Color correction and temperature
/// - Dithering mode
/// - RGBW conversion settings
struct ChannelConfig {

    // ========== New Variant-Based Constructors ==========

    /// @brief Named constructor with chipset variant
    /// @param name User-specified channel name
    /// @param chipset Chipset configuration (clockless or SPI)
    /// @param leds LED data array
    /// @param rgbOrder RGB channel ordering
    /// @param options Channel options (correction, temperature, dither, affinity)
    ChannelConfig(const fl::string& name, const ChipsetVariant& chipset, fl::span<CRGB> leds,
                  EOrder rgbOrder = RGB, const ChannelOptions& options = ChannelOptions()) FL_NOEXCEPT;

    /// @brief Primary constructor with chipset variant
    /// @param chipset Chipset configuration (clockless or SPI)
    /// @param leds LED data array
    /// @param rgbOrder RGB channel ordering
    /// @param options Channel options (correction, temperature, dither, affinity)
    ChannelConfig(const ChipsetVariant& chipset, fl::span<CRGB> leds,
                  EOrder rgbOrder = RGB, const ChannelOptions& options = ChannelOptions()) FL_NOEXCEPT;

    /// @brief Constructor with clockless chipset
    /// @param clockless Clockless chipset configuration
    /// @param leds LED data array
    /// @param rgbOrder RGB channel ordering
    /// @param options Channel options
    ChannelConfig(const ClocklessChipset& clockless, fl::span<CRGB> leds,
                  EOrder rgbOrder = RGB, const ChannelOptions& options = ChannelOptions()) FL_NOEXCEPT;

    /// @brief Constructor with SPI chipset
    /// @param spi SPI chipset configuration
    /// @param leds LED data array
    /// @param rgbOrder RGB channel ordering
    /// @param options Channel options
    ChannelConfig(const SpiChipsetConfig& spi, fl::span<CRGB> leds,
                  EOrder rgbOrder = RGB, const ChannelOptions& options = ChannelOptions()) FL_NOEXCEPT;

    // ========== Backwards-Compatible Constructors (Deprecated) ==========

    /// @brief Template constructor with TIMING type (backwards compatibility)
    /// @deprecated Use ClocklessChipset constructor instead
    /// @note This constructor creates a ClocklessChipset internally
    template<typename TIMING>
    ChannelConfig(int pin, fl::span<CRGB> leds, EOrder rgbOrder = RGB,
                  const ChannelOptions& options = ChannelOptions())
 FL_NOEXCEPT : ChannelConfig(makeClockless<TIMING>(pin), leds, rgbOrder, options) {}

    /// @brief Basic constructor with timing (backwards compatibility)
    /// @deprecated Use ClocklessChipset constructor instead
    /// @note This constructor creates a ClocklessChipset internally
    ChannelConfig(int pin, const ChipsetTimingConfig& timing, fl::span<CRGB> leds,
                  EOrder rgbOrder = RGB, const ChannelOptions& options = ChannelOptions()) FL_NOEXCEPT;

    // Copy constructor (needed because of variant)
    ChannelConfig(const ChannelConfig& other) FL_NOEXCEPT;

    // Move constructor (needed because of variant)
    ChannelConfig(ChannelConfig&& other) FL_NOEXCEPT;

    // Note: Assignment operators deleted because const members cannot be reassigned
    ChannelConfig& operator=(const ChannelConfig&) FL_NOEXCEPT = delete;
    ChannelConfig& operator=(ChannelConfig&&) FL_NOEXCEPT = delete;

    // ========== Accessors ==========

    /// @brief Get the chipset configuration variant
    const ChipsetVariant& getChipset() const FL_NOEXCEPT { return chipset; }

    /// @brief Check if this is a clockless chipset
    bool isClockless() const FL_NOEXCEPT { return chipset.is<ClocklessChipset>(); }

    /// @brief Check if this is an SPI chipset
    bool isSpi() const FL_NOEXCEPT { return chipset.is<SpiChipsetConfig>(); }

    /// @brief Get clockless chipset (returns nullptr if not clockless)
    const ClocklessChipset* getClocklessChipset() const FL_NOEXCEPT { return chipset.ptr<ClocklessChipset>(); }

    /// @brief Get SPI chipset (returns nullptr if not SPI)
    const SpiChipsetConfig* getSpiChipset() const FL_NOEXCEPT { return chipset.ptr<SpiChipsetConfig>(); }

    /// @brief Get data pin (works for both clockless and SPI)
    int getDataPin() const FL_NOEXCEPT;

    /// @brief Get clock pin (returns -1 for clockless chipsets)
    int getClockPin() const FL_NOEXCEPT;

    /// @brief Set screen map for JS canvas visualization
    /// @param map ScreenMap with LED positions
    void setScreenMap(const fl::ScreenMap& map) FL_NOEXCEPT { mScreenMap = map; }

    /// @brief Get screen map for JS canvas visualization
    /// @return Reference to current screen map
    const fl::ScreenMap& getScreenMap() const FL_NOEXCEPT { return mScreenMap; }

    /// @brief Check if screen map is configured
    /// @return true if screen map has been set with LEDs
    bool hasScreenMap() const FL_NOEXCEPT { return mScreenMap.getLength() > 0; }

    // ========== Data Members ==========

    /// Chipset configuration (clockless or SPI)
    ChipsetVariant chipset;

    /// LED data array
    fl::span<CRGB> mLeds;

    /// RGB channel ordering
    EOrder rgb_order = RGB;

    /// Optional channel settings (correction, temperature, dither, rgbw, affinity)
    ChannelOptions options;

    /// Screen mapping
    fl::ScreenMap mScreenMap;

    /// Optional user-specified name (if not set, Channel generates one automatically)
    fl::optional<fl::string> mName;
};

FASTLED_SHARED_PTR_STRUCT(ChannelConfig);

/// @brief Strongly-typed channel configuration with compile-time chipset family.
///
/// `ChannelConfigOf<Chipset>` is the Phase 3b (issue #2428) templated form of
/// `ChannelConfig`. It carries the same payload as a `ChannelConfig` but encodes
/// the chipset family (`ClocklessChipset` or `SpiChipsetConfig`) as a template
/// parameter so that `FastLED.add<Bus, Chipset>(cfg)` can:
///
///  - Deduce `Chipset` from the configuration without a runtime variant probe.
///  - `static_assert` that the named `Bus` supports `Chipset` at compile time
///    via `BusSupports<B, Chipset>`.
///  - Reject nonsense combinations (e.g. clockless config routed to an
///    SPI-only bus) at compile time rather than at runtime.
///
/// Implicit conversions to/from the non-template `ChannelConfig` preserve
/// backward compatibility -- existing `Channel::create(cfg)` callers and the
/// non-template `FastLED.add(cfg)` overloads continue to work unchanged.
///
/// @tparam Chipset One of `fl::ClocklessChipset` or `fl::SpiChipsetConfig`.
template<typename Chipset>
struct ChannelConfigOf {
    /// @brief Construct from a typed chipset, LEDs span, and optional metadata.
    ChannelConfigOf(const Chipset& chipset, fl::span<CRGB> leds,
                    EOrder rgbOrder = RGB,
                    const ChannelOptions& options = ChannelOptions()) FL_NOEXCEPT
        : chipset(chipset), mLeds(leds), rgb_order(rgbOrder), options(options) {}

    /// @brief Named-channel constructor.
    ChannelConfigOf(const fl::string& name, const Chipset& chipset,
                    fl::span<CRGB> leds, EOrder rgbOrder = RGB,
                    const ChannelOptions& options = ChannelOptions()) FL_NOEXCEPT
        : chipset(chipset), mLeds(leds), rgb_order(rgbOrder), options(options), mName(name) {}

    /// @brief Implicit conversion to the type-erased `ChannelConfig` so the
    ///        existing non-template `Channel::create()` factory accepts a
    ///        templated config without any per-call-site change.
    operator ChannelConfig() const FL_NOEXCEPT {
        ChannelConfig cfg(chipset, mLeds, rgb_order, options);
        cfg.mScreenMap = mScreenMap;
        if (mName.has_value()) {
            cfg.mName = mName;
        }
        return cfg;
    }

    /// @brief Convenience: build the erased form by value (handy in templates
    ///        where the implicit conversion is suppressed by overload rules).
    ChannelConfig toErased() const FL_NOEXCEPT { return static_cast<ChannelConfig>(*this); }

    // ---- Data members (mirror ChannelConfig, but with typed `chipset`) ----

    /// Typed chipset configuration. No variant -- the `Chipset` template
    /// parameter is the source of truth.
    Chipset chipset;

    /// LED data span.
    fl::span<CRGB> mLeds;

    /// RGB channel ordering.
    EOrder rgb_order = RGB;

    /// Optional channel settings (correction, temperature, dither, rgbw, affinity).
    ChannelOptions options;

    /// Screen mapping (for JS canvas visualization).
    fl::ScreenMap mScreenMap;

    /// Optional user-specified name. If unset, `Channel` auto-generates one.
    fl::optional<fl::string> mName;
};

/// @brief Multi-channel LED configuration
///
/// Stores shared pointers to ChannelConfig objects for managing multiple channels.
///
/// @example
/// ```cpp
/// MultiChannelConfig config;
/// auto channel1 = fl::make_shared<ChannelConfig>(pin1, timing);
/// auto channel2 = fl::make_shared<ChannelConfig>(pin2, timing);
/// config.add(channel1);
/// config.add(channel2);
/// ```
struct MultiChannelConfig {
    MultiChannelConfig() FL_NOEXCEPT = default;
    MultiChannelConfig(const MultiChannelConfig&) FL_NOEXCEPT = default;
    MultiChannelConfig(MultiChannelConfig&&) FL_NOEXCEPT = default;

    MultiChannelConfig(fl::span<ChannelConfigPtr> channels) FL_NOEXCEPT : mChannels(channels.begin(), channels.end()) {}
    MultiChannelConfig(fl::initializer_list<ChannelConfigPtr> channels) FL_NOEXCEPT : mChannels(channels.begin(), channels.end()) {}

    /// @brief Construct from span of ChannelConfig (copies to shared_ptr internally)
    /// @param channels Span of ChannelConfig objects to copy
    MultiChannelConfig(fl::span<ChannelConfig> channels) FL_NOEXCEPT;

    /// @brief Construct from initializer list of ChannelConfig (copies to shared_ptr internally)
    /// @param channels Initializer list of ChannelConfig objects to copy
    MultiChannelConfig(fl::initializer_list<ChannelConfig> channels) FL_NOEXCEPT;

    MultiChannelConfig& operator=(const MultiChannelConfig&) FL_NOEXCEPT = default;
    MultiChannelConfig& operator=(MultiChannelConfig&&) FL_NOEXCEPT = default;
    ~MultiChannelConfig() FL_NOEXCEPT = default;

    /// Add a channel configuration to the multi-channel config
    /// @param channel shared pointer to the channel configuration to add
    /// @returns reference to this object for method chaining
    MultiChannelConfig& add(ChannelConfigPtr channel) FL_NOEXCEPT;

    /// Vector of shared pointers to channel configurations
    fl::vector<ChannelConfigPtr> mChannels;
};

}
