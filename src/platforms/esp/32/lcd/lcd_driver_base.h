/// @file lcd_driver_base.h
/// @brief Base class for ESP32 LCD drivers (I80 and RGB)
///
/// This base class extracts common functionality from template-based LCD drivers,
/// reducing code duplication and improving maintainability.

#pragma once

#include "fl/stdint.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"

#include "crgb.h"
#include "platforms/shared/clockless_timing.h"
#include "lcd_driver_common.h"

namespace fl {

/// @brief Base class for LCD drivers containing non-template-dependent functionality
///
/// This class provides common functionality for both I80 and RGB LCD drivers,
/// including buffer management, strip attachment, synchronization, and timing queries.
class LcdDriverBase {
public:
    /// @brief Constructor
    LcdDriverBase()
        : num_leds_(0)
        , strips_{}
        , buffers_{nullptr, nullptr}
        , buffer_size_(0)
        , front_buffer_(0)
        , xfer_done_sem_(nullptr)
        , dma_busy_(false)
        , frame_counter_(0)
    {
        for (int i = 0; i < 16; i++) {
            strips_[i] = nullptr;
        }
    }

    /// @brief Destructor
    virtual ~LcdDriverBase() {
        // Cleanup handled by derived classes
    }

    /// @brief Attach per-lane LED strip data
    ///
    /// @param strips Array of CRGB pointers
    /// @param num_lanes Number of lanes to attach (must be <= 16)
    void attachStrips(CRGB** strips, int num_lanes) {
        for (int i = 0; i < num_lanes && i < 16; i++) {
            strips_[i] = strips[i];
        }
    }

    /// @brief Attach single strip to specific lane
    ///
    /// @param lane Lane index (0 to 15)
    /// @param strip Pointer to LED data array
    void attachStrip(int lane, CRGB* strip) {
        if (lane >= 0 && lane < 16) {
            strips_[lane] = strip;
        }
    }

    /// @brief Check if DMA transfer is in progress
    ///
    /// @return true if busy, false if idle
    bool busy() const { return dma_busy_; }

    /// @brief Block until current DMA transfer completes
    void wait() {
        if (dma_busy_) {
            // Wait for transfer to complete (with timeout)
            xSemaphoreTake(xfer_done_sem_, portMAX_DELAY);
            // Semaphore will be given by DMA callback or show()
            xSemaphoreGive(xfer_done_sem_);  // Re-give for next wait
        }
    }

    /// @brief Get buffer memory usage (bytes, per buffer)
    size_t getBufferSize() const {
        return buffer_size_;
    }

protected:
    /// @brief Helper function: Transpose 16 bytes into 16-bit words (one bit per lane)
    ///
    /// Input: 16 bytes (one per lane)
    /// Output: 8 uint16_t words representing bits 7-0 across 16 lanes
    ///
    /// This is the core transpose operation used by both I80 and RGB drivers
    /// to convert column-major LED data into row-major bit-parallel format.
    static inline void transpose16x1(const uint8_t* __restrict__ A, uint16_t* __restrict__ B) {
        uint32_t x;

        // Load 16 bytes as 4 uint32_t words
        const uint32_t* A32 = reinterpret_cast<const uint32_t*>(A);
        uint32_t a0 = A32[0];
        uint32_t a1 = A32[1];
        uint32_t a2 = A32[2];
        uint32_t a3 = A32[3];

        // Bit manipulation magic for transposing 16x8 bits
        // This converts column-major (16 bytes) to row-major (8 words of 16 bits each)

        // Phase 1: 16-bit shuffles
        x = (a0 & 0xFFFF0000) | (a1 >> 16);
        a1 = (a0 << 16) | (a1 & 0x0000FFFF);
        a0 = x;

        x = (a2 & 0xFFFF0000) | (a3 >> 16);
        a3 = (a2 << 16) | (a3 & 0x0000FFFF);
        a2 = x;

        // Phase 2: 8-bit shuffles
        x = (a0 & 0xFF00FF00) | ((a1 >> 8) & 0x00FF00FF);
        a1 = ((a0 << 8) & 0xFF00FF00) | (a1 & 0x00FF00FF);
        a0 = x;

        x = (a2 & 0xFF00FF00) | ((a3 >> 8) & 0x00FF00FF);
        a3 = ((a2 << 8) & 0xFF00FF00) | (a3 & 0x00FF00FF);
        a2 = x;

        // Phase 3: 4-bit shuffles
        x = (a0 & 0xF0F0F0F0) | ((a1 >> 4) & 0x0F0F0F0F);
        a1 = ((a0 << 4) & 0xF0F0F0F0) | (a1 & 0x0F0F0F0F);
        a0 = x;

        x = (a2 & 0xF0F0F0F0) | ((a3 >> 4) & 0x0F0F0F0F);
        a3 = ((a2 << 4) & 0xF0F0F0F0) | (a3 & 0x0F0F0F0F);
        a2 = x;

        // Phase 4: 2-bit shuffles
        x = (a0 & 0xCCCCCCCC) | ((a1 >> 2) & 0x33333333);
        a1 = ((a0 << 2) & 0xCCCCCCCC) | (a1 & 0x33333333);
        a0 = x;

        x = (a2 & 0xCCCCCCCC) | ((a3 >> 2) & 0x33333333);
        a3 = ((a2 << 2) & 0xCCCCCCCC) | (a3 & 0x33333333);
        a2 = x;

        // Phase 5: 1-bit shuffles
        x = (a0 & 0xAAAAAAAA) | ((a1 >> 1) & 0x55555555);
        a1 = ((a0 << 1) & 0xAAAAAAAA) | (a1 & 0x55555555);
        a0 = x;

        x = (a2 & 0xAAAAAAAA) | ((a3 >> 1) & 0x55555555);
        a3 = ((a2 << 1) & 0xAAAAAAAA) | (a3 & 0x55555555);
        a2 = x;

        // Extract 8 words (bits 7-0 across 16 lanes)
        uint32_t* B32 = reinterpret_cast<uint32_t*>(B);
        B32[0] = a0;
        B32[1] = a2;
        B32[2] = a1;
        B32[3] = a3;
    }

    // Common member variables
    int num_leds_;
    CRGB* strips_[16];

    // DMA buffers (double-buffered)
    uint16_t* buffers_[2];
    size_t buffer_size_;
    int front_buffer_;

    // Synchronization
    SemaphoreHandle_t xfer_done_sem_;
    volatile bool dma_busy_;
    uint32_t frame_counter_;
};

}  // namespace fl
