/// @file channel.h
/// @brief ESP32-P4 Parallel IO (PARLIO) LED channel
///
/// This driver uses the ESP32-P4 PARLIO TX peripheral to drive up to 16
/// identical WS28xx-style LED strips in parallel with DMA-based hardware timing.
///
/// Key features:
/// - Simultaneous output to multiple LED strips
/// - DMA-based write operation (minimal CPU overhead)
/// - Hardware timing control (no CPU bit-banging)
/// - Runtime-configured for different channel counts and chipset timings

#pragma once

#include "cpixel_ledcontroller.h"
#include "pixel_controller.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/weak_ptr.h"
#include "fl/stl/stdint.h"
#include "fl/string.h"
#include "fl/singleton.h"
#include "pixeltypes.h"
#include "fl/channels/config.h"

namespace fl {

// Forward declarations
class IChannelEngine;
class ChannelData;
struct ChannelOptions;
class Channel;
FASTLED_SHARED_PTR(Channel);
FASTLED_SHARED_PTR(ChannelData);

/// @brief LED channel for parallel output, pretty much a CPixelLEDController
///        but with timing and pin information.
///
/// Provides access to LED channel functionality for driving LED strips.
/// RGB_ORDER is set to RGB - reordering is handled internally by the Channel
class Channel: protected CPixelLEDController<RGB> {
public:
    /// @brief Create a new channel with optional affinity binding
    /// @param config Channel configuration (includes optional affinity for engine selection)
    /// @return Shared pointer to channel (auto-cleanup when out of scope)
    /// @note Channels always use ChannelBusManager by default
    /// @note If config.affinity is set, binds to the named engine from ChannelBusManager
    static ChannelPtr create(const ChannelConfig& config);

    /// @brief Destructor
    virtual ~Channel();

    /// @brief Get the channel ID
    /// @return Channel ID (always increments, starts at 0)
    i32 id() const { return mId; }

    /// @brief Get the channel name
    /// @return Channel name (user-specified or auto-generated "Channel_<id>")
    const fl::string& name() const { return mName; }

    /// @brief Get the pin number for this channel (data pin)
    /// @return Pin number
    int getPin() const { return mPin; }

    /// @brief Get the clock pin for this channel (SPI only, -1 for clockless)
    /// @return Clock pin number or -1
    int getClockPin() const;

    /// @brief Get the timing configuration for this channel (clockless only)
    /// @return ChipsetTimingConfig reference
    /// @deprecated Use getChipset() instead
    const ChipsetTimingConfig& getTiming() const { return mTiming; }

    /// @brief Get the chipset configuration variant
    /// @return ChipsetVariant reference
    const ChipsetVariant& getChipset() const { return mChipset; }

    /// @brief Get the RGB channel ordering
    /// @return EOrder value
    EOrder getRgbOrder() const { return mRgbOrder; }

    /// @brief Check if this is a clockless chipset
    bool isClockless() const { return mChipset.is<ClocklessChipset>(); }

    /// @brief Check if this is an SPI chipset
    bool isSpi() const { return mChipset.is<SpiChipsetConfig>(); }

    /// @brief Apply reconfigurable settings from a ChannelConfig
    /// @param config The configuration to apply
    /// @note Does NOT change: mPin, mTiming, mChipset, mEngine, mId
    void applyConfig(const ChannelConfig& config);

    // Re-expose protected base class methods for external access
    /// @brief Add this channel to the global controller draw list
    void addToDrawList();

    /// @brief Remove this channel from the global controller draw list
    void removeFromDrawList();

    /// @brief Get the number of LEDs in this channel
    int size() const override;

    /// @brief Show the LEDs with optional brightness scaling
    void showLeds(u8 brightness = 255) override;

    /// @brief Check if this channel is in the controller draw list
    bool isInDrawList() const;

    /// @brief Get pointer to base CLEDController for linked list traversal
    CLEDController* asController() { return static_cast<CLEDController*>(this); }
    const CLEDController* asController() const { return static_cast<const CLEDController*>(this); }

    /// @brief Get the LED array as a span (non-const)
    fl::span<CRGB> leds();

    /// @brief Get the LED array as a span (const)
    fl::span<const CRGB> leds() const;

    /// @brief Get the color correction
    CRGB getCorrection();

    /// @brief Get the color temperature
    CRGB getTemperature();

    /// @brief Get the dither mode
    u8 getDither();

    /// @brief Get the RGBW conversion mode
    Rgbw getRgbw() const;

    /// @brief Get the name of the currently bound engine (if any)
    /// @return Engine name, or empty string if no engine is bound or engine has expired
    fl::string getEngineName() const;

private:
    // CPixelLEDController interface implementation
    virtual void showPixels(PixelController<RGB, 1, 0xFFFFFFFF>& pixels) override;
    virtual void init() override;

    /// @brief Friend declaration for make_shared to access private constructor
    template<typename T, typename... Args>
    friend fl::shared_ptr<T> fl::make_shared(Args&&... args);

    /// @brief Private constructor (use create() factory method)
    /// @param chipset Chipset configuration (clockless or SPI)
    /// @param leds LED data array
    /// @param rgbOrder RGB channel ordering
    /// @param options Channel options (correction, temperature, dither, rgbw, affinity)
    Channel(const ChipsetVariant& chipset, fl::span<CRGB> leds,
            EOrder rgbOrder, const ChannelOptions& options);

    /// @brief Backwards-compatible constructor (deprecated)
    /// @deprecated Use variant-based constructor instead
    Channel(int pin, const ChipsetTimingConfig& timing, fl::span<CRGB> leds,
            EOrder rgbOrder, const ChannelOptions& options);

    // Non-copyable, non-movable
    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;
    Channel(Channel&&) = delete;
    Channel& operator=(Channel&&) = delete;

    static i32 nextId();
    static fl::string makeName(i32 id, const fl::optional<fl::string>& configName = fl::optional<fl::string>());

    ChipsetVariant mChipset;         // Chipset configuration (clockless or SPI)
    int mPin;                        // Data pin (backwards compatibility)
    ChipsetTimingConfig mTiming;     // Timing (backwards compatibility, clockless only)
    EOrder mRgbOrder;
    fl::weak_ptr<IChannelEngine> mEngine;  // Weak reference to engine (prevents dangling pointers)
    fl::string mAffinity;            // Engine affinity name (empty = no affinity, dynamic selection)
    const i32 mId;
    fl::string mName;               // User-specified or auto-generated name
    ChannelDataPtr mChannelData;
};

/// @brief Get stub channel engine for testing or unsupported platforms
/// @return Pointer to singleton stub engine instance
/// @note Returns a no-op engine that allows code to compile/run on all platforms
IChannelEngine* getStubChannelEngine();

}  // namespace fl
