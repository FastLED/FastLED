/// @file spi_hw_4_nrf52.h
/// @brief NRF52840 implementation of Quad-SPI using SPIM peripherals
///
/// This file provides the SPIQuadNRF52 class for Nordic nRF52840/52833 microcontrollers.
/// Uses four SPIM instances (SPIM0 + SPIM1 + SPIM2 + SPIM3) for quad-lane SPI transmission.
///
/// Hardware approach:
/// - SPIM0 for lane 0 (D0)
/// - SPIM1 for lane 1 (D1)
/// - SPIM2 for lane 2 (D2)
/// - SPIM3 for lane 3 (D3)
/// - TIMER + PPI for synchronized clock generation
/// - EasyDMA for zero-CPU transfers
///
/// Platform support:
/// - nRF52840: SPIM0/1/2 @ 8 MHz max, SPIM3 @ 32 MHz max
/// - nRF52833: SPIM0/1/2 @ 8 MHz max, SPIM3 @ 32 MHz max
/// - nRF52832: NOT SUPPORTED (only has SPIM0/1/2)
///
/// @note Only available on nRF52840 and nRF52833 (requires SPIM3)
/// @note All class definition and implementation is contained in this single file

#pragma once

// IWYU pragma: private

#include "platforms/arm/nrf52/is_nrf52.h"

// Only compile on nRF52840/52833 (requires SPIM3 for quad-lane operation)
#if defined(FL_IS_NRF52840) || defined(FL_IS_NRF52833)

#include "platforms/shared/spi_hw_4.h"
#include "fl/numeric_limits.h"
// IWYU pragma: begin_keep
#include <nrf_spim.h>
#include <nrf_gpio.h>
// IWYU pragma: end_keep

// Include Nordic SDK headers for TIMER, PPI
// IWYU pragma: begin_keep
#include <nrfx_timer.h>
#include <nrfx_ppi.h>
// IWYU pragma: end_keep

#include "fl/warn.h"
#include "fl/stddef.h"
#include "fl/stl/string.h"

namespace fl {

// ============================================================================
// SPIQuadNRF52 Class Definition
// ============================================================================

/// NRF52840 hardware driver for Quad-SPI DMA transmission using SPIM peripherals
///
/// Implements SpiHw4 interface for Nordic nRF52840/52833 platforms using:
/// - SPIM0 + SPIM1 + SPIM2 + SPIM3 for quad-lane data transmission
/// - TIMER1 for synchronization trigger (via PPI)
/// - EasyDMA for non-blocking asynchronous transfers
/// - Configurable clock frequency up to 8 MHz (SPIM0-2) or 32 MHz (SPIM3)
///
/// @note Each instance allocates four SPIM peripherals
/// @note Requires EasyDMA buffers in RAM (not flash)
/// @note Uses PPI channels 4-7 for synchronization
/// @note Only available on nRF52840/52833 (requires SPIM3)
class SPIQuadNRF52 : public SpiHw4 {
public:
    /// @brief Construct a new SPIQuadNRF52 controller
    /// @param bus_id Logical bus identifier (0 or 1)
    /// @param name Human-readable name for this controller
    explicit SPIQuadNRF52(int bus_id = -1, const char* name = "Unknown");

    /// @brief Destroy the controller and release all resources
    ~SPIQuadNRF52();

    /// @brief Initialize the SPI controller with specified configuration
    /// @param config Configuration including pins, clock speed, and bus number
    /// @return true if initialization successful, false on error
    /// @note Validates pin assignments and allocates SPIM/TIMER/PPI resources
    bool begin(const SpiHw4::Config& config) override;

    /// @brief Deinitialize the controller and release resources
    void end() override;

    /// @brief Acquire a DMA buffer for zero-copy data preparation
    /// @param bytes_per_lane Number of bytes per lane to allocate
    /// @return DMABuffer containing span to buffer or error code
    /// @note Automatically waits if previous transmission still active
    /// @note Reallocates only if requested size exceeds current capacity
    DMABuffer acquireDMABuffer(size_t bytes_per_lane) override;

    /// @brief Start non-blocking transmission using internal DMA buffer
    /// @return true if transfer started successfully, false on error
    /// @note Must call acquireDMABuffer() first
    /// @note Returns immediately - use waitComplete() to block until done
    bool transmit(TransmitMode mode = TransmitMode::ASYNC) override;

    /// @brief Wait for current transmission to complete
    /// @param timeout_ms Maximum time to wait in milliseconds (fl::numeric_limits<uint32_t>::max() = infinite)
    /// @return true if transmission completed, false on timeout
    bool waitComplete(u32 timeout_ms = fl::numeric_limits<u32>::max()) override;

    /// @brief Check if transmission is currently in progress
    /// @return true if busy, false if idle
    bool isBusy() const override;

    /// @brief Check if controller has been initialized
    /// @return true if initialized, false otherwise
    bool isInitialized() const override;

    /// @brief Get the bus identifier for this controller
    /// @return Bus ID (0 or 1)
    int getBusId() const override;

    /// @brief Get the human-readable name for this controller
    /// @return Controller name string
    const char* getName() const override;

private:
    /// @brief Release all allocated resources (SPIM, TIMER, PPI, buffers)
    void cleanup();

    /// @brief Allocate or resize internal DMA buffers for quad-lane operation
    /// @param required_size Size needed in bytes (per lane)
    /// @return true if buffers allocated successfully
    bool allocateDMABuffers(size_t required_size);

    /// @brief Configure TIMER1 for synchronization trigger
    /// @param clock_speed_hz Desired clock frequency
    void configureTimer(u32 clock_speed_hz);

    /// @brief Configure PPI channels for synchronization
    void configurePPI();

    /// @brief Start synchronized transmission on all SPIM peripherals
    void startTransmission();

    int mBusId;  ///< Logical bus identifier
    const char* mName;  ///< Human-readable controller name

    // SPIM resources
    NRF_SPIM_Type* mSPIM0;  ///< SPIM0 peripheral (lane 0)
    NRF_SPIM_Type* mSPIM1;  ///< SPIM1 peripheral (lane 1)
    NRF_SPIM_Type* mSPIM2;  ///< SPIM2 peripheral (lane 2)
    NRF_SPIM_Type* mSPIM3;  ///< SPIM3 peripheral (lane 3)

    // TIMER resource
    NRF_TIMER_Type* mTimer;  ///< TIMER1 for synchronization

    // DMA buffer management (must be in RAM for EasyDMA)
    DMABuffer mDMABuffer;            ///< Allocated DMA buffer (interleaved format for quad-lane)
    size_t mMaxBytesPerLane;         ///< Max bytes per lane we've allocated for
    size_t mCurrentTotalSize;        ///< Current transmission size (bytes_per_lane * num_lanes)
    bool mBufferAcquired;            ///< True if buffer has been acquired for transmission

    // Legacy lane buffers (kept for internal de-interleaving)
    u8* mLane0Buffer;  ///< Buffer for lane 0 data
    u8* mLane1Buffer;  ///< Buffer for lane 1 data
    u8* mLane2Buffer;  ///< Buffer for lane 2 data
    u8* mLane3Buffer;  ///< Buffer for lane 3 data
    size_t mBufferSize;     ///< Size of each lane buffer in bytes

    // State
    bool mTransactionActive;  ///< True if transmission in progress
    bool mInitialized;        ///< True if controller initialized

    // Configuration
    u8 mClockPin;  ///< Clock pin (SCK)
    u8 mData0Pin;  ///< Data pin for lane 0
    u8 mData1Pin;  ///< Data pin for lane 1
    u8 mData2Pin;  ///< Data pin for lane 2
    u8 mData3Pin;  ///< Data pin for lane 3
    u32 mClockSpeed;  ///< Clock frequency in Hz

    // PPI channels used (4-7)
    u8 mPPIChannel4;  ///< PPI channel for TIMER -> SPIM0.START
    u8 mPPIChannel5;  ///< PPI channel for TIMER -> SPIM1.START
    u8 mPPIChannel6;  ///< PPI channel for TIMER -> SPIM2.START
    u8 mPPIChannel7;  ///< PPI channel for TIMER -> SPIM3.START

    SPIQuadNRF52(const SPIQuadNRF52&) = delete;
    SPIQuadNRF52& operator=(const SPIQuadNRF52&) = delete;
};

}  // namespace fl

#endif  // FL_IS_NRF52840 || FL_IS_NRF52833
