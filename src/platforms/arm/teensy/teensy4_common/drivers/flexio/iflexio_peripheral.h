/// @file iflexio_peripheral.h
/// @brief Abstract interface for FlexIO peripheral (real hardware and mock)
///
/// This interface abstracts FlexIO2 hardware operations so the channel engine
/// can be tested with a mock peripheral on host platforms.

#pragma once

// IWYU pragma: private

#include "fl/stl/stdint.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// @brief Abstract interface for FlexIO2 LED driver operations
///
/// Concrete implementations:
/// - FlexIOPeripheralReal: Real FlexIO2 hardware on Teensy 4.x
/// - FlexIOPeripheralMock: Mock for host-based unit testing
class IFlexIOPeripheral {
public:
    virtual ~IFlexIOPeripheral() = default;

    /// @brief Check if a Teensy pin has a FlexIO2 mapping
    /// @param teensy_pin Teensy digital pin number
    /// @return true if pin can be used with FlexIO2
    virtual bool canHandlePin(u8 teensy_pin) const FL_NOEXCEPT = 0;

    /// @brief Initialize FlexIO2 for a specific pin and timing
    /// @param teensy_pin Teensy digital pin number
    /// @param t0h_ns T0H timing in nanoseconds
    /// @param t1h_ns T1H timing in nanoseconds (t1_ns + t2_ns)
    /// @param period_ns Total bit period in nanoseconds
    /// @param reset_us Reset/latch time in microseconds
    /// @return true on success
    virtual bool init(u8 teensy_pin, u32 t0h_ns, u32 t1h_ns,
                      u32 period_ns, u32 reset_us) FL_NOEXCEPT = 0;

    /// @brief Start DMA transfer of pixel data
    /// @param pixel_data Encoded pixel bytes
    /// @param num_bytes Number of bytes
    /// @return true if transfer started
    virtual bool show(const u8* pixel_data, u32 num_bytes) FL_NOEXCEPT = 0;

    /// @brief Check if DMA transfer is complete
    /// @return true if done or no transfer active
    virtual bool isDone() const FL_NOEXCEPT = 0;

    /// @brief Block until transfer completes
    virtual void wait() FL_NOEXCEPT = 0;

    /// @brief Shut down and release resources
    virtual void deinit() FL_NOEXCEPT = 0;

    /// @brief Get the singleton peripheral instance (platform-specific)
    static fl::shared_ptr<IFlexIOPeripheral> create() FL_NOEXCEPT;

protected:
    IFlexIOPeripheral() = default;
    IFlexIOPeripheral(const IFlexIOPeripheral&) = delete;
    IFlexIOPeripheral& operator=(const IFlexIOPeripheral&) = delete;
};

} // namespace fl
