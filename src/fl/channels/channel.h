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
#include "fl/stl/stdint.h"
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
class Channel: public CPixelLEDController<RGB> {
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
    int32_t id() const { return mId; }

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

    /// @brief Check if this is a clockless chipset
    bool isClockless() const { return mChipset.is<ClocklessChipset>(); }

    /// @brief Check if this is an SPI chipset
    bool isSpi() const { return mChipset.is<SpiChipsetConfig>(); }

    /// @brief Get the channel engine this channel belongs to
    /// @return Pointer to the IChannelEngine, or nullptr if not set
    IChannelEngine* getChannelEngine() const { return mEngine; }

    /// @brief Set the channel engine this channel belongs to
    /// @param engine Pointer to the IChannelEngine (singleton, not owned)
    void setChannelEngine(IChannelEngine* engine) { mEngine = engine; }

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
    /// @param engine Channel engine pointer
    /// @param options Channel options (correction, temperature, dither, rgbw)
    Channel(const ChipsetVariant& chipset, fl::span<CRGB> leds,
            EOrder rgbOrder, IChannelEngine* engine, const ChannelOptions& options);

    /// @brief Backwards-compatible constructor (deprecated)
    /// @deprecated Use variant-based constructor instead
    Channel(int pin, const ChipsetTimingConfig& timing, fl::span<CRGB> leds,
            EOrder rgbOrder, IChannelEngine* engine, const ChannelOptions& options);

    // Non-copyable, non-movable
    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;
    Channel(Channel&&) = delete;
    Channel& operator=(Channel&&) = delete;

    static int32_t nextId();

    ChipsetVariant mChipset;         // Chipset configuration (clockless or SPI)
    const int mPin;                  // Data pin (backwards compatibility)
    const ChipsetTimingConfig mTiming; // Timing (backwards compatibility, clockless only)
    const EOrder mRgbOrder;
    IChannelEngine* mEngine;         // Singleton pointer, not owned
    const int32_t mId;
    ChannelDataPtr mChannelData;
};

/// @brief Get stub channel engine for testing or unsupported platforms
/// @return Pointer to singleton stub engine instance
/// @note Returns a no-op engine that allows code to compile/run on all platforms
IChannelEngine* getStubChannelEngine();

}  // namespace fl
