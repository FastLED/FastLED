/// @file iobjectfled_peripheral.h
/// @brief Abstract interface for ObjectFLED peripheral (real hardware and mock)
///
/// This interface abstracts ObjectFLED DMA hardware operations so the channel
/// engine can be tested with a mock peripheral on host platforms.
///
/// Unlike FlexIO (one pin at a time), ObjectFLED drives all pins in parallel
/// per timing group. The interface abstracts at the timing-group level.

#pragma once

// IWYU pragma: private

#include "fl/stl/stdint.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// @brief Result of pin validation
struct ObjectFLEDPinResult {
    bool valid;
    const char* error_message;
};

/// @brief Abstract handle for a single ObjectFLED timing-group instance
///
/// Owns a frame buffer that the channel engine writes pixel data into.
/// show() triggers synchronous DMA transmission.
class IObjectFLEDInstance {
public:
    virtual ~IObjectFLEDInstance() = default;

    /// @brief Get pointer to the frame buffer for writing pixel data
    virtual u8* getFrameBuffer() FL_NOEXCEPT = 0;

    /// @brief Get size of the frame buffer in bytes
    virtual u32 getFrameBufferSize() const FL_NOEXCEPT = 0;

    /// @brief Trigger synchronous DMA transmission
    virtual void show() FL_NOEXCEPT = 0;

protected:
    IObjectFLEDInstance() = default;
    IObjectFLEDInstance(const IObjectFLEDInstance&) = delete;
    IObjectFLEDInstance& operator=(const IObjectFLEDInstance&) = delete;
};

/// @brief Abstract factory/validator for ObjectFLED peripheral
///
/// Concrete implementations:
/// - ObjectFLEDPeripheralReal: Real ObjectFLED hardware on Teensy 4.x
/// - ObjectFLEDPeripheralMock: Mock for host-based unit testing
class IObjectFLEDPeripheral {
public:
    virtual ~IObjectFLEDPeripheral() = default;

    /// @brief Validate whether a pin can be used with ObjectFLED
    /// @param pin Teensy digital pin number
    /// @return Validation result with error message if invalid
    virtual ObjectFLEDPinResult validatePin(u8 pin) const FL_NOEXCEPT = 0;

    /// @brief Create an ObjectFLED instance for a timing group
    /// @param totalLeds Total number of LEDs across all pins in this group
    /// @param isRgbw true if RGBW mode (4 bytes/LED), false for RGB (3 bytes/LED)
    /// @param numPins Number of pins in this group
    /// @param pinList Array of pin numbers
    /// @param t1_ns T1 timing in nanoseconds
    /// @param t2_ns T2 timing in nanoseconds
    /// @param t3_ns T3 timing in nanoseconds
    /// @param reset_us Reset time in microseconds
    /// @return Instance handle, or nullptr on failure
    virtual fl::unique_ptr<IObjectFLEDInstance> createInstance(
        int totalLeds, bool isRgbw, u32 numPins, const u8* pinList,
        u32 t1_ns, u32 t2_ns, u32 t3_ns, u32 reset_us) FL_NOEXCEPT = 0;

    /// @brief Get the platform-specific peripheral instance
    static fl::shared_ptr<IObjectFLEDPeripheral> create() FL_NOEXCEPT;

protected:
    IObjectFLEDPeripheral() = default;
    IObjectFLEDPeripheral(const IObjectFLEDPeripheral&) = delete;
    IObjectFLEDPeripheral& operator=(const IObjectFLEDPeripheral&) = delete;
};

} // namespace fl
