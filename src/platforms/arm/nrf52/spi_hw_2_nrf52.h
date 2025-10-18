/// @file spi_hw_2_nrf52.h
/// @brief NRF52 implementation of Dual-SPI using SPIM peripherals
///
/// This file provides the SPIDualNRF52 class for Nordic nRF52 series microcontrollers.
/// Uses two SPIM instances (SPIM0 + SPIM1) for dual-lane SPI transmission.
///
/// Hardware approach:
/// - SPIM0 for lane 0 (D0)
/// - SPIM1 for lane 1 (D1)
/// - TIMER + PPI + GPIOTE for synchronized clock generation
/// - EasyDMA for zero-CPU transfers
///
/// Platform support:
/// - nRF52832: SPIM0/1 @ 8 MHz max
/// - nRF52840: SPIM0/1 @ 8 MHz max
///
/// @note All class definition and implementation is contained in this single file

#pragma once

#if defined(NRF52) || defined(NRF52832) || defined(NRF52840) || defined(NRF52833)

#include "platforms/shared/spi_hw_2.h"
#include <nrf_spim.h>
#include <nrf_gpio.h>

// Include Nordic SDK headers for TIMER, PPI, GPIOTE
#if defined(NRF52840) || defined(NRF52833)
    #include <nrfx_timer.h>
    #include <nrfx_ppi.h>
    #include <nrfx_gpiote.h>
#else
    // For nRF52832, use legacy API
    #include <nrf_timer.h>
    #include <nrf_ppi.h>
    #include <nrf_gpiote.h>
#endif

#include "fl/warn.h"
#include "fl/stddef.h"
#include "fl/str.h"

namespace fl {

// ============================================================================
// SPIDualNRF52 Class Definition
// ============================================================================

/// NRF52 hardware driver for Dual-SPI DMA transmission using SPIM peripherals
///
/// Implements SpiHw2 interface for Nordic nRF52 platforms using:
/// - SPIM0 + SPIM1 for dual-lane data transmission
/// - TIMER0 for clock generation (via PPI/GPIOTE)
/// - EasyDMA for non-blocking asynchronous transfers
/// - Configurable clock frequency up to 8 MHz (nRF52832) or 32 MHz (SPIM3 on nRF52840)
///
/// @note Each instance allocates two SPIM peripherals
/// @note Requires EasyDMA buffers in RAM (not flash)
/// @note Uses PPI channels 0-2 for synchronization
/// @note Uses GPIOTE channel 0 for clock output
class SPIDualNRF52 : public SpiHw2 {
public:
    /// @brief Construct a new SPIDualNRF52 controller
    /// @param bus_id Logical bus identifier (0 or 1)
    /// @param name Human-readable name for this controller
    explicit SPIDualNRF52(int bus_id = -1, const char* name = "Unknown");

    /// @brief Destroy the controller and release all resources
    ~SPIDualNRF52();

    /// @brief Initialize the SPI controller with specified configuration
    /// @param config Configuration including pins, clock speed, and bus number
    /// @return true if initialization successful, false on error
    /// @note Validates pin assignments and allocates SPIM/TIMER/PPI resources
    bool begin(const SpiHw2::Config& config) override;

    /// @brief Deinitialize the controller and release resources
    void end() override;

    /// @brief Start non-blocking transmission of data buffer
    /// @param buffer Data to transmit (reorganized into dual-lane format internally)
    /// @return true if transfer started successfully, false on error
    /// @note Waits for previous transaction to complete if still active
    /// @note Returns immediately - use waitComplete() to block until done
    bool transmitAsync(fl::span<const uint8_t> buffer) override;

    /// @brief Wait for current transmission to complete
    /// @param timeout_ms Maximum time to wait in milliseconds (UINT32_MAX = infinite)
    /// @return true if transmission completed, false on timeout
    bool waitComplete(uint32_t timeout_ms = UINT32_MAX) override;

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
    /// @brief Release all allocated resources (SPIM, TIMER, PPI, GPIOTE, buffers)
    void cleanup();

    /// @brief Allocate or resize internal DMA buffers for dual-lane operation
    /// @param required_size Size needed in bytes (per lane)
    /// @return true if buffers allocated successfully
    bool allocateDMABuffers(size_t required_size);

    /// @brief Configure TIMER0 for clock generation
    /// @param clock_speed_hz Desired clock frequency
    void configureTimer(uint32_t clock_speed_hz);

    /// @brief Configure PPI channels for synchronization
    void configurePPI();

    /// @brief Configure GPIOTE for clock output
    void configureGPIOTE();

    /// @brief Start synchronized transmission on both SPIM peripherals
    void startTransmission();

    int mBusId;  ///< Logical bus identifier
    const char* mName;  ///< Human-readable controller name

    // SPIM resources
    NRF_SPIM_Type* mSPIM0;  ///< SPIM0 peripheral (lane 0)
    NRF_SPIM_Type* mSPIM1;  ///< SPIM1 peripheral (lane 1)

    // TIMER resource
    NRF_TIMER_Type* mTimer;  ///< TIMER0 for clock generation

    // DMA buffers (must be in RAM for EasyDMA)
    uint8_t* mLane0Buffer;  ///< Buffer for lane 0 data
    uint8_t* mLane1Buffer;  ///< Buffer for lane 1 data
    size_t mBufferSize;     ///< Size of each lane buffer in bytes

    // State
    bool mTransactionActive;  ///< True if transmission in progress
    bool mInitialized;        ///< True if controller initialized

    // Configuration
    uint8_t mClockPin;  ///< Clock pin (SCK)
    uint8_t mData0Pin;  ///< Data pin for lane 0
    uint8_t mData1Pin;  ///< Data pin for lane 1
    uint32_t mClockSpeed;  ///< Clock frequency in Hz

    // PPI channels used (0-2)
    uint8_t mPPIChannel0;  ///< PPI channel for TIMER -> GPIOTE (clock)
    uint8_t mPPIChannel1;  ///< PPI channel for TIMER -> SPIM0.START
    uint8_t mPPIChannel2;  ///< PPI channel for TIMER -> SPIM1.START

    SPIDualNRF52(const SPIDualNRF52&) = delete;
    SPIDualNRF52& operator=(const SPIDualNRF52&) = delete;
};

}  // namespace fl

#endif  // NRF52 variants
