/// @file parlio_transmitter.h
/// @brief PARLIO transmitter for broadcasting to multiple LED strips with same chipset timing
///
/// This file provides the transmitter layer that coordinates parallel output to multiple
/// LED channels (strips) sharing identical chipset timing. Each unique chipset timing gets
/// its own singleton transmitter instance via a static factory function, enabling independent
/// management and multi-chipset support.
///
/// **Architecture:**
/// - ParlioTransmitter: Transmits to K channels with identical timing
/// - Static factory getOrCreate<CHIPSET>() converts compile-time timing to runtime
/// - Each unique timing config gets an independent singleton transmitter instance
/// - ParlioTransmitterBase contains actual implementation (in clockless_parlio_esp32p4.cpp)
///
/// **Template Usage:**
/// Only the static factory function getOrCreate<CHIPSET>() is templated to convert
/// compile-time chipset types to runtime ChipsetTimingConfig values. After creation,
/// all operations use runtime parameters.

#pragma once

#if !defined(CONFIG_IDF_TARGET_ESP32P4)
#error "This file is only for ESP32-P4"
#endif

#include "fl/chipsets/chipset_timing_config.h"  // ChipsetTimingConfig, makeTimingConfig
#include "fl/singleton.h"                        // fl::Singleton
#include "fl/stdint.h"                           // uint8_t, uint16_t

namespace fl {

// Forward declarations
class PixelIterator;
template<typename T, typename Deleter> class unique_ptr;

/// @brief Abstract interface for PARLIO transmitter management
///
/// This interface provides singleton pattern for managing PARLIO LED channels (strips)
/// broadcasting with identical chipset timing. Each unique timing gets its own transmitter
/// instance via static factory function.
///
/// Implementation is hidden in .cpp file for complete platform isolation.
///
/// @example Basic usage:
/// ```cpp
/// // Get or create the WS2812 transmitter
/// auto& ws2812_tx = IParlioTransmitter::getOrCreate<TIMING_WS2812_800KHZ>();
///
/// // Add channels to this transmitter
/// ws2812_tx.onQueuingStart();
/// ws2812_tx.addStrip(2, 100, false);   // Pin 2, 100 LEDs, RGB
/// ws2812_tx.addStrip(5, 100, false);   // Pin 5, 100 LEDs, RGB
/// ws2812_tx.flush();
/// ```
class IParlioTransmitter {
public:
    /// @brief Get or create a singleton instance for a specific chipset (template version)
    ///
    /// This templated static function converts compile-time CHIPSET types to
    /// runtime ChipsetTimingConfig and returns/creates appropriate singleton.
    ///
    /// @tparam CHIPSET Chipset timing trait (e.g., TIMING_WS2812_800KHZ)
    /// @return Reference to the singleton IParlioTransmitter for this chipset
    template <typename CHIPSET>
    static IParlioTransmitter& getOrCreate();

    /// @brief Get or create a singleton instance for a runtime timing config
    ///
    /// This runtime version directly uses a ChipsetTimingConfig to find or create
    /// the appropriate singleton instance. Useful when chipset timing is determined
    /// at runtime rather than compile-time.
    ///
    /// @param timing Runtime chipset timing configuration
    /// @return Reference to the singleton IParlioTransmitter for this timing
    static IParlioTransmitter& getOrCreate(const ChipsetTimingConfig& timing);

    /// @brief Virtual destructor
    virtual ~IParlioTransmitter() = default;

    // ===== Frame lifecycle methods =====

    /// @brief Start queuing strips for a new frame
    ///
    /// Should be called once at the start of each frame before adding strips.
    /// Subsequent calls within the same frame are safely ignored.
    ///
    /// Frame lifecycle: IDLE → QUEUING → FLUSHED → IDLE
    virtual void onQueuingStart() = 0;

    /// @brief Check if currently queuing strips for a frame
    ///
    /// @return true if in QUEUING state, false otherwise
    virtual bool isQueuing() const = 0;

    /// @brief Notify that all strips have been queued
    ///
    /// Called after all strips for this frame have been added.
    virtual void onQueuingDone() = 0;

    // ===== Strip management methods =====

    /// @brief Add an LED strip to this group
    ///
    /// Registers a strip on the specified pin with the given configuration.
    /// All strips in a group must have:
    /// - Same LED count (PARLIO hardware limitation)
    /// - Same RGB/RGBW mode (consistency requirement)
    ///
    /// @param pin GPIO pin number (will be validated against ESP32-P4 constraints)
    /// @param numLeds Number of LEDs in the strip
    /// @param is_rgbw true for RGBW strips, false for RGB strips
    ///
    /// Constraints:
    /// - Maximum 16 strips per group (PARLIO hardware limit)
    /// - All strips must have identical LED counts
    /// - All strips must use same RGB/RGBW mode
    /// - Pin must be valid for PARLIO peripheral
    virtual void addStrip(uint8_t pin, uint16_t numLeds, bool is_rgbw) = 0;

    /// @brief Write pixel data for a specific strip
    ///
    /// Writes RGB or RGBW pixel data from a PixelIterator into the internal
    /// DMA buffer for the specified pin. Data is written with proper scaling and dithering
    /// as configured in the PixelIterator.
    ///
    /// @param data_pin GPIO pin number of the strip to write to
    /// @param pixel_iterator Iterator providing pixel data with scaling/dithering applied
    ///
    /// @note Must be called after addStrip() has registered the pin
    /// @note Automatically handles RGB vs RGBW mode based on pixel_iterator configuration
    virtual void writePixels(uint8_t data_pin, PixelIterator& pixel_iterator) = 0;

    // ===== Transmission methods =====

    /// @brief Flush queued strips and transmit data
    ///
    /// Configures the PARLIO driver for optimal width (1, 2, 4, 8, or 16 lanes)
    /// and initiates DMA-based transmission to all registered strips in parallel.
    ///
    /// This method:
    /// 1. Acquires PARLIO hardware (blocks if another driver is active)
    /// 2. Configures DMA buffers for parallel transmission
    /// 3. Starts async transmission
    /// 4. Releases hardware for next driver (DMA continues in background)
    ///
    /// Thread safety: Coordinated by IParlioEngine
    virtual void flush() = 0;

    /// @brief Alias for flush() - transmit all strips once per frame
    ///
    /// Same as flush(). Provided for compatibility with ObjectFLED naming.
    void showPixelsOnceThisFrame() {
        flush();
    }

protected:
    /// @brief Protected constructor (interface pattern)
    IParlioTransmitter() = default;

    // Prevent copying and assignment
    IParlioTransmitter(const IParlioTransmitter&) = delete;
    IParlioTransmitter& operator=(const IParlioTransmitter&) = delete;
    IParlioTransmitter(IParlioTransmitter&&) = delete;
    IParlioTransmitter& operator=(IParlioTransmitter&&) = delete;
};

} // namespace fl
