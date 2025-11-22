#include "fl/spi/parallel_device.h"
#include "fl/spi/device_impl_base.h"
#include "fl/warn.h"
#include "fl/dbg.h"

// Include parallel SPI backends
#include "platforms/shared/spi_bitbang/spi_isr_1.h"
#include "platforms/shared/spi_bitbang/spi_isr_2.h"
#include "platforms/shared/spi_bitbang/spi_isr_4.h"
#include "platforms/shared/spi_bitbang/spi_isr_8.h"
#include "platforms/shared/spi_bitbang/spi_isr_16.h"
#include "platforms/shared/spi_bitbang/spi_isr_32.h"
#include "platforms/shared/spi_bitbang/spi_block_1.h"
#include "platforms/shared/spi_bitbang/spi_block_2.h"
#include "platforms/shared/spi_bitbang/spi_block_4.h"
#include "platforms/shared/spi_bitbang/spi_block_8.h"
#include "platforms/shared/spi_bitbang/spi_block_16.h"
#include "platforms/shared/spi_bitbang/spi_block_32.h"

namespace fl {
namespace spi {

// ============================================================================
// Config Implementation (to avoid circular dependency with Spi class)
// ============================================================================

ParallelDevice::Config::Config()
    : clock_pin(0xFF)
    , mode(SpiParallelMode::AUTO)
    , timer_hz(1600000) {
}

// ============================================================================
// Implementation Details (pImpl pattern)
// ============================================================================

struct ParallelDevice::Impl {
    Config config;
    bool initialized;
    void* backend;  // Points to SpiIsr* or SpiBlock* (not owned by shared_ptr)
    bool is_isr_mode;
    uint8_t backend_width;  // 1, 2, 4, 8, 16, or 32

    Impl(const Config& cfg)
        : config(cfg)
        , initialized(false)
        , backend(nullptr)
        , is_isr_mode(false)
        , backend_width(0) {
    }

    ~Impl() {
        if (initialized && backend) {
            releaseBackend();
        }
    }

    void releaseBackend() {
        if (!backend) return;

        // Stop ISR if in ISR mode
        if (is_isr_mode) {
            // ISR backends have stopISR() method
            if (backend_width == 1) {
                static_cast<SpiIsr1*>(backend)->stopISR();
            } else if (backend_width == 2) {
                static_cast<SpiIsr2*>(backend)->stopISR();
            } else if (backend_width == 4) {
                static_cast<SpiIsr4*>(backend)->stopISR();
            } else if (backend_width == 8) {
                static_cast<SpiIsr8*>(backend)->stopISR();
            } else if (backend_width == 16) {
                static_cast<SpiIsr16*>(backend)->stopISR();
            } else if (backend_width == 32) {
                static_cast<SpiIsr32*>(backend)->stopISR();
            }
        }

        // Note: SpiIsr* and SpiBlock* backends don't need delete
        // (they're typically stack-allocated in this impl)
        backend = nullptr;
        initialized = false;
    }
};

// ============================================================================
// Helper Functions
// ============================================================================

namespace {

/// @brief Build default LUT for parallel GPIO mapping
/// @param gpio_pins Vector of GPIO pin numbers
/// @param set_masks Output array of 256 set masks
/// @param clear_masks Output array of 256 clear masks
void buildDefaultLUT(const fl::vector<uint8_t>& gpio_pins,
                     uint32_t* set_masks,
                     uint32_t* clear_masks) {
    size_t num_pins = gpio_pins.size();

    // For each possible byte value (0-255)
    for (int byte_val = 0; byte_val < 256; byte_val++) {
        uint32_t set_mask = 0;
        uint32_t clear_mask = 0;

        // Map byte bits to GPIO pins
        for (size_t bit_pos = 0; bit_pos < num_pins && bit_pos < 8; bit_pos++) {
            uint32_t pin_mask = 1u << gpio_pins[bit_pos];

            if (byte_val & (1 << bit_pos)) {
                set_mask |= pin_mask;    // Set this pin high
            } else {
                clear_mask |= pin_mask;  // Clear this pin low
            }
        }

        // For pins beyond bit 7, always clear them (if num_pins > 8)
        for (size_t pin_idx = 8; pin_idx < num_pins; pin_idx++) {
            clear_mask |= (1u << gpio_pins[pin_idx]);
        }

        set_masks[byte_val] = set_mask;
        clear_masks[byte_val] = clear_mask;
    }
}

} // anonymous namespace

// ============================================================================
// ParallelDevice Implementation
// ============================================================================

ParallelDevice::ParallelDevice(const Config& config)
    : pImpl(fl::make_unique<Impl>(config)) {

    // Validate configuration
    size_t num_pins = config.gpio_pins.size();
    if (num_pins == 0 || num_pins > 32) {
        FL_WARN("ParallelDevice: Invalid number of GPIO pins (" << num_pins
                << "), must be 1-32");
    }

    FL_DBG("ParallelDevice: Created with " << num_pins << " GPIO pins");
}

ParallelDevice::~ParallelDevice() {
    if (pImpl && pImpl->initialized) {
        end();
    }
}

fl::optional<fl::Error> ParallelDevice::begin() {
    if (!pImpl) {
        return fl::Error("Device not initialized");
    }

    if (pImpl->initialized) {
        // Already initialized - idempotent
        return fl::nullopt;
    }

    size_t num_pins = pImpl->config.gpio_pins.size();

    // Validate pin count
    if (num_pins == 0 || num_pins > 32) {
        return fl::Error("Invalid number of GPIO pins (must be 1-32)");
    }

    // Determine backend width (round up to next supported width: 1,2,4,8,16,32)
    uint8_t backend_width = 1;
    if (num_pins > 16) backend_width = 32;
    else if (num_pins > 8) backend_width = 16;
    else if (num_pins > 4) backend_width = 8;
    else if (num_pins > 2) backend_width = 4;
    else if (num_pins > 1) backend_width = 2;

    pImpl->backend_width = backend_width;

    // Determine execution mode
    bool use_isr = (pImpl->config.mode == SpiParallelMode::ISR_ASYNC) ||
                   (pImpl->config.mode == SpiParallelMode::AUTO);  // Default to ISR if AUTO
    pImpl->is_isr_mode = use_isr;

    // Build default LUT
    uint32_t set_masks[256];
    uint32_t clear_masks[256];
    buildDefaultLUT(pImpl->config.gpio_pins, set_masks, clear_masks);

    // Create and initialize backend
    // Note: We can't use polymorphism here due to different backend types
    // Instead, we use type-erasure with void* and manual dispatch

    if (use_isr) {
        // ISR-based backend
        FL_DBG("ParallelDevice: Initializing ISR mode (width=" << (int)backend_width << ")");

        // Note: These need to be heap-allocated for type-erasure
        // For now, return error indicating ISR mode not yet implemented
        return fl::Error("ISR mode not yet implemented for ParallelDevice");

    } else {
        // Bit-bang (blocking) backend
        FL_DBG("ParallelDevice: Initializing bit-bang mode (width=" << (int)backend_width << ")");

        // Note: These need to be heap-allocated for type-erasure
        // For now, return error indicating bit-bang mode not yet implemented
        return fl::Error("Bit-bang mode not yet implemented for ParallelDevice");
    }

    pImpl->initialized = true;
    return fl::nullopt;
}

void ParallelDevice::end() {
    if (!pImpl || !pImpl->initialized) {
        return;
    }

    // Wait for pending operations
    waitComplete();

    // Release backend
    pImpl->releaseBackend();

    FL_DBG("ParallelDevice: Shutdown complete");
}

bool ParallelDevice::isReady() const {
    return pImpl && pImpl->initialized && pImpl->backend != nullptr;
}

Result<Transaction> ParallelDevice::write(const uint8_t* data, size_t size) {
    if (!isReady()) {
        return Result<Transaction>::failure(SPIError::NOT_INITIALIZED,
            "Device not initialized");
    }

    if (!data || size == 0) {
        return Result<Transaction>::failure(SPIError::ALLOCATION_FAILED,
            "Invalid data or size");
    }

    // Write implementation depends on backend type
    // For now, return not implemented
    return Result<Transaction>::failure(SPIError::NOT_SUPPORTED,
        "ParallelDevice::write() not yet implemented");
}

bool ParallelDevice::waitComplete(uint32_t timeout_ms) {
    (void)timeout_ms;  // Unused parameter - reserved for future implementation
    if (!isReady()) {
        return false;
    }

    // Wait implementation depends on backend type
    // For ISR mode, check status flags
    // For bit-bang mode, transmission is already complete (blocking)

    return true;  // Placeholder
}

bool ParallelDevice::isBusy() const {
    if (!isReady()) {
        return false;
    }

    // Busy check depends on backend type
    return false;  // Placeholder
}

void ParallelDevice::configureLUT(const uint32_t* set_masks, const uint32_t* clear_masks) {
    if (!set_masks || !clear_masks) {
        FL_WARN("ParallelDevice: Invalid LUT pointers");
        return;
    }

    // LUT configuration depends on backend type
    // For now, this is a no-op
    FL_DBG("ParallelDevice: LUT configuration not yet implemented");
}

const ParallelDevice::Config& ParallelDevice::getConfig() const {
    static const Config empty_config;
    return pImpl ? pImpl->config : empty_config;
}

} // namespace spi
} // namespace fl
