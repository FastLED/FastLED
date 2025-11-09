/// @file parlio_group.h
/// @brief Interface for PARLIO write coordinator service
///
/// This is a pure write service that handles:
/// - DMA buffer packing and write operations
/// - Frame lifecycle management (IDLE → QUEUING → FLUSHED)
/// - Coordination with IParlioManager for batched write operations
///
/// Strip management (add/remove/get) happens in BulkClockless<PARLIO> specialization.
/// This service receives strip registrations and pixel data, then packs and writes.
///
/// @note This is an internal ESP32 platform header. Do not include directly.

#pragma once

#ifdef CONFIG_IDF_TARGET_ESP32P4

#include "fl/chipsets/chipset_timing_config.h"
#include "fl/chipsets/led_timing.h"  // makeTimingConfig
#include "fl/stdint.h"
#include "pixeltypes.h"

namespace fl {

// Forward declarations
class PixelIterator;
struct ParlioHardwareConfig;

/// @brief Abstract interface for PARLIO write coordinator
///
/// This is a pure service (DESIGN.md "Group" terminology) that handles DMA write operations.
/// Multiple LED strips with identical chipset timing share one instance (singleton per timing).
///
/// Responsibilities:
/// - Frame lifecycle management (IDLE → QUEUING → FLUSHED)
/// - Strip registration (registerStrip/unregisterStrip)
/// - Pixel data buffering (writePixels)
/// - DMA packing and write operations (via IParlioManager)
/// - Singleton factory pattern for managing instances by chipset timing
///
/// Implementation is hidden in .cpp file for complete platform isolation.
class IParlioGroup {
public:
    /// Maximum number of strips supported by PARLIO peripheral
    static constexpr int MAX_STRIPS = 16;

    // ===== Factory Methods (Singleton Pattern) =====

    /// @brief Get or create a singleton instance for a runtime timing config
    ///
    /// This runtime version directly uses a ChipsetTimingConfig to find or create
    /// the appropriate singleton instance. Useful when chipset timing is determined
    /// at runtime rather than compile-time.
    ///
    /// @param timing Runtime chipset timing configuration
    /// @return Reference to the singleton IParlioGroup for this timing
    static IParlioGroup& getOrCreate(const ChipsetTimingConfig& timing);

    /// @brief Get or create a singleton instance for a specific chipset (template version)
    ///
    /// This templated static function converts compile-time CHIPSET types to
    /// runtime ChipsetTimingConfig and returns/creates appropriate singleton.
    ///
    /// @tparam CHIPSET Chipset timing trait (e.g., TIMING_WS2812_800KHZ)
    /// @return Reference to the singleton IParlioGroup for this chipset
    template <typename CHIPSET>
    static IParlioGroup& getOrCreate() {
        // Convert compile-time CHIPSET to runtime config and delegate to runtime version
        return getOrCreate(makeTimingConfig<CHIPSET>());
    }

    /// @brief Virtual destructor
    virtual ~IParlioGroup() = default;

    // ===== Frame Lifecycle Methods =====

    /// @brief Start queuing strips for a new frame
    virtual void onQueuingStart() = 0;

    /// @brief Check if currently queuing strips for a frame
    virtual bool isQueuing() const = 0;

    /// @brief Notify that all strips have been queued
    virtual void onQueuingDone() = 0;

    // ===== Strip Registration Methods =====

    /// @brief Register a strip for write operations
    /// @param pin GPIO pin number
    /// @param buffer non-owning pointer to LED data (user-owned)
    /// @param count number of LEDs in this strip
    /// @param is_rgbw true for RGBW strips, false for RGB strips
    virtual void registerStrip(uint8_t pin, CRGB* buffer, int count, bool is_rgbw) = 0;

    /// @brief Unregister a strip from write operations
    /// @param pin GPIO pin number of strip to remove
    virtual void unregisterStrip(uint8_t pin) = 0;

    // ===== Data Writing Methods =====

    /// @brief Write pixel data for a specific strip
    /// @param data_pin GPIO pin number of the strip
    /// @param pixel_iterator Iterator providing pixel data with scaling/dithering
    virtual void writePixels(uint8_t data_pin, PixelIterator& pixel_iterator) = 0;

    // ===== Write Operations Methods =====

    /// @brief Flush queued strips and write data via hub
    virtual void flush() = 0;

    // ===== Hub Integration Methods =====

    /// @brief Prepare DMA buffer for write operation
    /// @note Full implementation in Task 3.1 (data packing logic)
    virtual void prepare_write() = 0;

    /// @brief Get hardware configuration for this group
    /// @return Hardware config for engine
    /// @note Full implementation in Task 3.1
    virtual ParlioHardwareConfig get_hardware_config() = 0;

    /// @brief Get DMA buffer pointer
    /// @return Pointer to packed DMA data
    /// @note Full implementation in Task 3.1
    virtual const uint8_t* get_dma_buffer() = 0;

    /// @brief Get total bits to write
    /// @return Total bit count for all strips
    /// @note Full implementation in Task 3.1
    virtual uint32_t get_total_bits() = 0;

    /// @brief Get bits per component (for chunk alignment)
    /// @return Bits per LED component
    /// @note Full implementation in Task 3.1
    virtual uint32_t get_bits_per_component() = 0;

protected:
    /// @brief Protected constructor (interface pattern)
    IParlioGroup() = default;

    // Non-copyable, non-movable
    IParlioGroup(const IParlioGroup&) = delete;
    IParlioGroup& operator=(const IParlioGroup&) = delete;
    IParlioGroup(IParlioGroup&&) = delete;
    IParlioGroup& operator=(IParlioGroup&&) = delete;
};

} // namespace fl

#endif // CONFIG_IDF_TARGET_ESP32P4
