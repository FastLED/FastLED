/// @file spi_channel_adapter.h
/// @brief Adapter that wraps HW SPI controllers for ChannelBusManager
///
/// This adapter enables existing SpiHw1/2/4/8/16 controllers to work with
/// the modern channel-based API while maintaining backward compatibility
/// with SPIBusManager. It uses the Adapter pattern to wrap platform-specific
/// hardware controllers without modification.
///
/// **Architecture Overview:**
///
/// ```
/// Application Code (APA102 strips)
///          ↓
///    ChannelBusManager (proxy)
///          ↓
///    SpiChannelEngineAdapter (this file)
///          ↓
///    SpiHw1/2/4/8/16 (existing, unchanged)
///          ↓
///    Platform Hardware (DMA, SPI peripheral)
/// ```
///
/// **Critical Distinction:**
/// This adapter is for **TRUE SPI chipsets** (APA102, SK9822, HD108) that
/// require synchronized clock signals. This is fundamentally different from
/// ChannelEngineSpi, which implements clockless protocols (WS2812) using
/// SPI hardware as a bit-banging engine.
///
/// | Adapter | Chipsets | Clock Pin Usage | Purpose |
/// |---------|----------|-----------------|---------|
/// | SpiChannelEngineAdapter | APA102, SK9822 | Connected to LEDs | True SPI |
/// | ChannelEngineSpi | WS2812, SK6812 | Internal only | Clockless-over-SPI |
///
/// **Priority Scheme:**
/// True SPI adapters are registered with higher priority than clockless engines:
/// - SPI_HEXADECA (priority 9): 16-lane true SPI
/// - SPI_OCTAL (priority 8): 8-lane true SPI
/// - SPI_QUAD (priority 7): 4-lane true SPI
/// - SPI_DUAL (priority 6): 2-lane true SPI
/// - SPI_SINGLE (priority 5): 1-lane true SPI
/// - PARLIO (priority 4): Clockless parallel I/O
/// - ... (lower priority clockless engines)

#pragma once

#include "fl/channels/engine.h"
#include "fl/channels/data.h"
#include "fl/stl/vector.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/string.h"
#include "platforms/shared/spi_hw_base.h"  // ok platform headers - required for SpiHwBase interface

namespace fl {

/// @brief Adapter that wraps HW SPI controllers for ChannelBusManager
///
/// This adapter implements the IChannelEngine interface by delegating to
/// existing SpiHwBase controllers (SpiHw1/2/4/8/16). It handles channel data
/// batching, transmission coordination, and polling state management.
class SpiChannelEngineAdapter : public IChannelEngine {
public:
    /// @brief Create unified adapter managing multiple controllers
    /// @param hwControllers Vector of hardware controllers (SpiHw1, SpiHw16, etc.)
    /// @param priorities Per-controller priorities (must match hwControllers.size())
    /// @param names Per-controller names (must match hwControllers.size())
    /// @param adapterName Unified adapter name (e.g., "SPI_UNIFIED")
    /// @returns Shared pointer to adapter, or nullptr on error
    static fl::shared_ptr<SpiChannelEngineAdapter> create(
        fl::vector<fl::shared_ptr<SpiHwBase>> hwControllers,
        fl::vector<int> priorities,
        fl::vector<const char*> names,
        const char* adapterName
    );

    /// @brief Destructor
    ~SpiChannelEngineAdapter() override;

    // IChannelEngine interface implementation

    /// @brief Check if adapter can handle channel data
    /// @param data Channel data to check
    /// @return true if data is true SPI chipset (APA102, SK9822, HD108)
    /// @note Rejects clockless chipsets (WS2812, SK6812, etc.)
    bool canHandle(const ChannelDataPtr& data) const override;

    /// @brief Enqueue channel data for transmission
    /// @param channelData Channel data to transmit
    /// @note Batches multiple channels for later transmission
    void enqueue(ChannelDataPtr channelData) override;

    /// @brief Trigger transmission of enqueued data
    /// @note Groups channels by clock pin, acquires DMA buffers, calls transmit()
    void show() override;

    /// @brief Query engine state and perform maintenance
    /// @return Current engine state (READY, BUSY, DRAINING, or ERROR)
    /// @note Checks isBusy(), releases buffers when complete
    EngineState poll() override;

    /// @brief Get adapter name for debugging
    /// @return Engine name (e.g., "SPI_SINGLE")
    fl::string getName() const override { return mName; }

    /// @brief Get engine capabilities (SPI protocols only)
    /// @return Capabilities with supportsSpi=true, supportsClockless=false
    Capabilities getCapabilities() const override {
        return Capabilities(false, true);  // SPI only
    }

    /// @brief Get maximum priority among all controllers
    /// @return Highest priority (5-9 for true SPI adapters)
    int getPriority() const;

private:
    /// @brief Friend declaration for make_shared to access private constructor
    template<typename T, typename... Args>
    friend fl::shared_ptr<T> fl::make_shared(Args&&... args);

    /// @brief Private constructor - use create() factory method
    SpiChannelEngineAdapter(const char* name);

    /// @brief Information about a registered SPI hardware controller
    struct ControllerInfo {
        fl::shared_ptr<SpiHwBase> controller;  ///< Hardware instance
        int priority;                          ///< Controller priority (higher = preferred)
        fl::string name;                       ///< Name (e.g., "SPI2", "SPI3", "I2S0")
        fl::vector<int> assignedClockPins;     ///< Clock pins assigned to this controller
        bool isInitialized;                    ///< Whether begin() has been called

        ControllerInfo(fl::shared_ptr<SpiHwBase> ctrl, int prio, const char* n)
            : controller(ctrl), priority(prio), name(n), isInitialized(false) {}
    };

    struct ClockPinAssignment {
        int clockPin;
        size_t controllerIndex;  ///< Index into mControllers
    };

    /// @brief Select best controller for a given clock pin
    /// @param clockPin Clock pin number to route
    /// @returns Controller index, or -1 if none available
    int selectControllerForClockPin(int clockPin);

    /// @brief Check if controller can handle this clock pin
    /// @param ctrl Controller to check
    /// @param clockPin Clock pin to test
    /// @returns true if compatible, false otherwise
    bool canControllerHandleClockPin(const ControllerInfo& ctrl, int clockPin) const;

    /// @brief Initialize controller if needed for this clock pin
    /// @param ctrl Controller to initialize
    /// @param clockPin Clock pin for configuration
    /// @param dataPin Data pin for configuration
    /// @returns true on success, false on error
    bool initializeControllerIfNeeded(ControllerInfo& ctrl, int clockPin, int dataPin);

    /// @brief Group data structure for channels with same clock pin
    struct ClockPinGroup {
        int clockPin;
        fl::vector<ChannelDataPtr> channels;
    };

    /// @brief Group channels by clock pin for efficient transmission
    /// @param channels Span of channel data to group
    /// @return Vector of groups (clock pin + channels)
    /// @note Channels with same clock pin can share SPI bus configuration
    fl::vector<ClockPinGroup> groupByClockPin(
        fl::span<const ChannelDataPtr> channels
    );

    /// @brief Transmit a batch of channels (all same clock pin)
    /// @param channels Channels to transmit (must have same clock pin)
    /// @return true on success, false on error
    bool transmitBatch(fl::span<const ChannelDataPtr> channels);

    fl::vector<ControllerInfo> mControllers;              ///< All managed controllers
    fl::vector<ClockPinAssignment> mClockPinAssignments;  ///< Clock pin → controller mapping
    fl::string mName;                                    ///< Engine name for debugging

    fl::vector<ChannelDataPtr> mEnqueuedChannels;      ///< Channels waiting for show()
    fl::vector<ChannelDataPtr> mTransmittingChannels;  ///< Channels currently transmitting

    // Non-copyable, non-movable
    SpiChannelEngineAdapter(const SpiChannelEngineAdapter&) = delete;
    SpiChannelEngineAdapter& operator=(const SpiChannelEngineAdapter&) = delete;
    SpiChannelEngineAdapter(SpiChannelEngineAdapter&&) = delete;
    SpiChannelEngineAdapter& operator=(SpiChannelEngineAdapter&&) = delete;
};

} // namespace fl
