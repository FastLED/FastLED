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

#include "fl/stl/noexcept.h"

#include "cpixel_ledcontroller.h"
#include "fl/channels/bus.h"
#include "fl/channels/ichannel.h"
#include "fl/channels/options.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/string.h"
#include "fl/stl/weak_ptr.h"
#include "fl/stl/stdint.h"
#include "fl/channels/config.h"
#include "fl/stl/compiler_control.h"

namespace fl {

// Forward declarations
class IChannelDriver;
class ChannelData;
struct ChannelOptions;  // IWYU pragma: keep
class Channel;
class XMap;
FASTLED_SHARED_PTR(Channel);
FASTLED_SHARED_PTR(ChannelData);

/// @brief LED channel for parallel output, pretty much a CPixelLEDController
///        but with timing and pin information.
///
/// Provides access to LED channel functionality for driving LED strips.
/// RGB_ORDER is set to RGB - reordering is handled internally by the Channel.
///
/// Inherits `IChannel` so `ChannelEvents` callbacks (and other consumers that
/// only need to identify a channel) can take a non-template reference even
/// after `Channel<Bus, Chipset>` becomes templated in Phase 3b. See #2428.
class Channel: public CPixelLEDController<RGB>, public IChannel {
public:
    /// @brief Create a new channel with optional `mBus` driver pinning.
    /// @param config Channel configuration. If `config.options.mBus` is not
    ///        `Bus::AUTO`, the channel pins itself to the driver named by
    ///        `busName(mBus)` via `ChannelManager::findDriverByName()`. On a
    ///        miss, `showPixels()` falls back to AUTO/priority dispatch and
    ///        emits a one-shot `FL_ERROR` (see channel.cpp.hpp).
    /// @return Shared pointer to channel (auto-cleanup when out of scope)
    /// @note Channels always use ChannelManager by default.
    static ChannelPtr create(const ChannelConfig& config);

    /// @brief Destructor
    virtual ~Channel() FL_NOEXCEPT;

    /// @brief Get the channel ID
    /// @return Channel ID (always increments, starts at 0)
    i32 id() const override { return mId; }

    /// @brief Get the channel name
    /// @return Channel name (user-specified or auto-generated "Channel_<id>")
    const fl::string& name() const override { return mName; }

    /// @brief Get the pin number for this channel (data pin)
    /// @return Pin number
    int getPin() const;

    /// @brief Get the clock pin for this channel (SPI only, -1 for clockless)
    /// @return Clock pin number or -1
    int getClockPin() const;

    /// @brief Set gamma correction value
    /// @param gamma Gamma value (e.g., 2.8)
    /// @return Reference to this channel for chaining
    Channel& setGamma(float gamma);

    /// @brief Get gamma correction value
    /// @return Gamma value if set, nullopt otherwise
    fl::optional<float> getGamma() const;

    /// @brief Get the timing configuration for this channel (clockless only)
    /// @return ChipsetTimingConfig reference
    /// @deprecated Use getChipset() instead
    const ChipsetTimingConfig& getTiming() const;

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
    /// @note Does NOT change: mChipset, mDriver, mId
    void applyConfig(const ChannelConfig& config);

    // Re-expose protected base class methods for external access
    /// @brief Add this channel to the global controller draw list
    void addToDrawList();

    /// @brief Remove this channel from the global controller draw list
    void removeFromDrawList();

    /// @brief Get the number of LEDs in this channel
    int size() const override;

    /// @brief Show the LEDs with optional brightness scaling
    void showLeds(u8 brightness = 255) OVERRIDE_IF_NOT_AVR;

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

    /// @brief Get the name of the currently bound driver (if any)
    /// @return Engine name, or empty string if no driver is bound or driver has expired
    fl::string getEngineName() const;

    /// @brief Set screen map for JS canvas visualization from XYMap
    /// @param map 2D addressing mode (serpentine, line-by-line, etc.)
    /// @param diameter Optional LED diameter for canvas rendering
    /// @return Reference to this channel for method chaining
    Channel& setScreenMap(const fl::XYMap& map, float diameter = -1.f);

    /// @brief Set screen map for JS canvas visualization
    /// @param map ScreenMap with LED positions
    /// @return Reference to this channel for method chaining
    Channel& setScreenMap(const fl::ScreenMap& map);

    /// @brief Set screen map for JS canvas visualization from dimensions
    /// @param width Grid width in pixels
    /// @param height Grid height in pixels
    /// @param diameter Optional LED diameter for canvas rendering
    /// @return Reference to this channel for method chaining
    Channel& setScreenMap(fl::u16 width, fl::u16 height, float diameter = -1.f);

    /// @brief Set screen map for 1D strip remapping from XMap
    /// @param map 1D addressing mode (linear, reverse, custom function, LUT)
    /// @return Reference to this channel for method chaining
    Channel& setScreenMap(const fl::XMap& map);

    /// @brief Get the current screen map
    /// @return Reference to current screen map
    const fl::ScreenMap& getScreenMap() const;

    /// @brief Check if screen map is configured
    /// @return true if screen map has been set
    bool hasScreenMap() const;

private:
    /// @brief Friend declaration for make_shared to access private constructor
    template<typename T, typename... Args>
    friend fl::shared_ptr<T> fl::make_shared(Args&&... args) FL_NOEXCEPT;

protected:
    // CPixelLEDController interface implementation - protected so subclass delegates
    // (e.g., UCS7604's DelegateController) can call through the base class chain.
    void showPixels(PixelController<RGB, 1, 0xFFFFFFFF>& pixels) override;
    void init() override;
    /// @brief Protected constructor for template subclasses (e.g., ClocklessIdf5)
    /// @param chipset Chipset configuration (clockless or SPI)
    /// @param rgbOrder RGB channel ordering
    /// @param mode Registration mode (AutoRegister or DeferRegister)
    /// @note Does not set LED data or channel options - caller must do that
    Channel(const ChipsetVariant& chipset, EOrder rgbOrder, RegistrationMode mode);

private:
    /// @brief Cold slow-path helper for `showPixels()` when the driver was
    ///        NOT pre-bound via `setDriver()`. Handles dynamic
    ///        `ChannelManager::selectDriverForChannel()` lookup AND the
    ///        bus-key-miss diagnostics (#2455 / #2459).
    ///
    /// Marked `FL_NO_INLINE` so the legacy `addLeds<>` hot path stays compact —
    /// see #2773 item 2.1. Returns `nullptr` on a hard miss (caller should
    /// silently bail).
    FL_NO_INLINE fl::shared_ptr<IChannelDriver> resolveDynamicDriver();
protected:

    /// @brief Pre-bind a driver, bypassing `ChannelManager::selectDriverForChannel()`
    ///        on every subsequent `showPixels()` call.
    ///
    /// Used by legacy `addLeds<>`-style controllers (e.g. `ClocklessIdf5`) to
    /// route directly to a `BusTraits<DefaultBus>::instancePtr()` singleton at
    /// construction time. Post-#2428 this is the mechanism that lets
    /// `--gc-sections` drop unreferenced driver TUs from default builds
    /// (Phase 5b — the binary-size fix for #2420 / #2421).
    ///
    /// @note Stored as `weak_ptr` to avoid holding the driver alive past the
    ///       caller's intent. The caller (typically the static singleton in a
    ///       BusTraits) owns the strong reference.
    void setDriver(fl::shared_ptr<IChannelDriver> driver) FL_NOEXCEPT {
        mDriver = driver;
        mDriverPreBound = true;
    }

private:
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
    Channel(const Channel&) FL_NOEXCEPT = delete;
    Channel& operator=(const Channel&) FL_NOEXCEPT = delete;
    Channel(Channel&&) FL_NOEXCEPT = delete;
    Channel& operator=(Channel&&) FL_NOEXCEPT = delete;

    static i32 nextId();
    static fl::string makeName(i32 id, const fl::optional<fl::string>& configName = fl::optional<fl::string>());

    ChipsetVariant mChipset;         // Chipset configuration (clockless or SPI)
    EOrder mRgbOrder;
    fl::weak_ptr<IChannelDriver> mDriver;  // Weak reference to driver (prevents dangling pointers)
    bool mDriverPreBound = false;    // True if setDriver() was called (legacy addLeds<> path).
                                     // When true, showPixels() uses mDriver directly and skips
                                     // ChannelManager::selectDriverForChannel(). When false (the
                                     // default), every showPixels() re-evaluates the manager's
                                     // priority list so users can swap drivers at runtime.
    Bus mBus = Bus::AUTO;            // Typed driver selection (#2459). `Bus::AUTO` falls through
                                     // to `ChannelManager` priority dispatch; any other value
                                     // pins this channel to `busName(mBus)`.
    bool mBusWarned = false;         // One-shot guard for the #2455 FL_ERROR. Flipped on the
                                     // first showPixels() that observes a `mBus` miss (the
                                     // named driver wasn't registered with ChannelManager).
                                     // Subsequent shows on the same channel skip the warn even
                                     // though the priority-dispatch fallback re-runs every frame.
    bool mDisabledDriverWarned = false;  // One-shot guard for the #2517 FL_ERROR. Flipped on the
                                         // first showPixels() that would have enqueued data to a
                                         // driver disabled by `setExclusiveDriver(...)` (or
                                         // `setDriverEnabled(name, false)`). Cleared whenever the
                                         // resolved driver returns to ENABLED so a subsequent
                                         // disable re-emits the diagnostic.
    const i32 mId;
    fl::string mName;               // User-specified or auto-generated name
    ChannelOptions mSettings;           // Per-channel settings (gamma, rgbw, etc.)
    ChannelDataPtr mChannelData;
    fl::ScreenMap mScreenMap;        // Screen map for JS canvas visualization
};

/// @brief Get stub channel driver for testing or unsupported platforms
/// @return Pointer to singleton stub driver instance
/// @note Returns a no-op driver that allows code to compile/run on all platforms
IChannelDriver* getStubChannelEngine();

}  // namespace fl
