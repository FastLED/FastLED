#include "fl/spi.h"
#include "fl/spi/impl.h"
#include "fl/log.h"
#include "fl/warn.h"
#include "fl/stl/time.h"
#include "platforms/shared/spi_bus_manager.h"
#include "platforms/shared/spi_hw_1.h"
#include "platforms/shared/spi_hw_2.h"
#include "platforms/shared/spi_hw_4.h"

namespace fl {
namespace spi {

// ============================================================================
// Device Implementation
// ============================================================================

Device::Device(const Config& config)
    : pImpl(fl::make_unique<Impl>(config)) {
    FL_LOG_SPI("SPI Device: Created with clock=" << config.clock_pin
           << " data_pins.size()=" << config.data_pins.size());
}

Device::~Device() {
    FL_LOG_SPI("SPI Device: Destructor called");
    if (pImpl && pImpl->initialized) {
        FL_LOG_SPI("SPI Device: Calling end() from destructor");
        end();
    }

    FL_LOG_SPI("SPI Device: Checking owned hardware backend");
    // Clean up owned hardware backend (for SINGLE_SPI mode)
    // Note: hw_backend is now a shared_ptr, so cleanup is automatic
    if (pImpl && pImpl->owns_backend && pImpl->hw_backend) {
        FL_LOG_SPI("SPI Device: Cleaning up owned hardware backend");
        pImpl->hw_backend->end();
        pImpl->hw_backend = nullptr;
    }
    FL_LOG_SPI("SPI Device: Destructor complete");
}

fl::optional<fl::Error> Device::begin() {
    if (!pImpl) {
        return fl::Error("Device not initialized");
    }

    if (pImpl->initialized) {
        // Already initialized - idempotent
        return fl::nullopt;
    }

    // Validate SPI mode (0-3 for CPOL/CPHA combinations)
    if (pImpl->config.spi_mode > 3) {
        FL_WARN("SPI Device: Invalid SPI mode " << pImpl->config.spi_mode << " (must be 0-3)");
        return fl::Error("Invalid SPI mode");
    }

    // Note: SPI mode configuration is not yet supported by the hardware layer
    // All devices currently operate in mode 0 (CPOL=0, CPHA=0)
    if (pImpl->config.spi_mode != 0) {
        FL_WARN("SPI Device: SPI mode " << pImpl->config.spi_mode
                << " requested but hardware layer only supports mode 0 - ignoring");
    }

    // Register with SPIBusManager
    SPIBusManager& mgr = getSPIBusManager();
    int data_pin = pImpl->config.data_pins.empty() ? -1 : pImpl->config.data_pins[0];
    pImpl->bus_handle = mgr.registerDevice(
        pImpl->config.clock_pin,
        data_pin,
        pImpl->config.clock_speed_hz,
        this  // controller pointer
    );

    if (!pImpl->bus_handle.is_valid) {
        FL_WARN("SPI Device: Failed to register with bus manager");
        return fl::Error("Failed to register with bus manager");
    }

    // Initialize the bus
    if (!mgr.initialize()) {
        FL_WARN("SPI Device: Bus initialization failed");
        return fl::Error("Bus initialization failed");
    }

    // Check if we need to create our own SpiHw1 controller (SINGLE_SPI/passthrough mode)
    const SPIBusInfo* bus_info = mgr.getBusInfo(pImpl->bus_handle.bus_id);
    if (bus_info && bus_info->bus_type == SPIBusType::SINGLE_SPI && !bus_info->hw_controller) {
        // Create and initialize a SpiHw1 controller
        const fl::vector<fl::shared_ptr<SpiHw1>>& controllers = SpiHw1::getAll();
        if (controllers.empty()) {
            FL_WARN("SPI Device: No SpiHw1 controllers available on this platform");
            return fl::Error("No SpiHw1 controllers available");
        }

        // Use the first available controller (could be improved with bus number selection)
        fl::shared_ptr<SpiHw1> hw = controllers[0];

        SpiHw1::Config hw_config;
        hw_config.clock_pin = pImpl->config.clock_pin;
        hw_config.data_pin = pImpl->config.data_pins.empty() ? -1 : pImpl->config.data_pins[0];
        hw_config.clock_speed_hz = pImpl->config.clock_speed_hz;
        hw_config.bus_num = 0;  // Default to bus 0

        if (!hw->begin(hw_config)) {
            FL_WARN("SPI Device: Failed to initialize SpiHw1 controller");
            return fl::Error("Failed to initialize SpiHw1");
        }

        pImpl->hw_backend = hw;
        pImpl->owns_backend = false;  // We don't own it (it's from the static pool)
        FL_LOG_SPI("SPI Device: Created SpiHw1 controller for SINGLE_SPI mode");
    } else {
        // Multi-lane or hardware controller already exists
        pImpl->hw_backend = bus_info ? bus_info->hw_controller : nullptr;
        pImpl->owns_backend = false;
    }

    pImpl->initialized = true;
    FL_LOG_SPI("SPI Device: Initialized successfully");
    return fl::nullopt;
}

void Device::end() {
    if (!pImpl || !pImpl->initialized) {
        return;
    }

    // Wait for any pending operations
    waitComplete();

    // Note: Do NOT call hw->end() here!
    // The SPIBusManager will call releaseBusHardware() (which calls hw->end())
    // when the last device on this bus is unregistered below.
    // Calling hw->end() here would result in calling end() twice on the same hardware.
    pImpl->hw_backend = nullptr;

    // Unregister from bus manager
    if (pImpl->bus_handle.is_valid) {
        SPIBusManager& mgr = getSPIBusManager();
        mgr.unregisterDevice(pImpl->bus_handle);
        pImpl->bus_handle = SPIBusHandle();
    }

    pImpl->initialized = false;
    FL_LOG_SPI("SPI Device: Shutdown complete");
}

bool Device::isReady() const {
    return pImpl && pImpl->initialized;
}

// ============================================================================
// Unsupported RX Operations (TX-Only SPI Design)
// ============================================================================

// Commented out - methods not declared in header
// fl::optional<fl::Error> Device::read(uint8_t* buffer, size_t size) {
//     FL_UNUSED(buffer);
//     FL_UNUSED(size);
//
//     FL_WARN("SPI Device: read() not supported - FastLED uses TX-only SPI for LED strips");
//     return fl::Error(SPIError::NOT_SUPPORTED,
//         "Read operations not supported - LED strips are TX-only (no MISO/readback)");
// }

// fl::optional<fl::Error> Device::transfer(const uint8_t* tx_data, uint8_t* rx_buffer, size_t size) {
//     FL_UNUSED(tx_data);
//     FL_UNUSED(rx_buffer);
//     FL_UNUSED(size);
//
//     FL_WARN("SPI Device: transfer() not supported - FastLED uses TX-only SPI. Use writeAsync() instead.");
//     return fl::Error(SPIError::NOT_SUPPORTED,
//         "Full-duplex transfer not supported - LED strips are TX-only. Use writeAsync() for transmission.");
// }

// ============================================================================
// Transaction API (Primary Interface)
// ============================================================================

Result<Transaction> Device::writeAsync(const uint8_t* data, size_t size) {
    if (!isReady()) {
        return Result<Transaction>::failure(SPIError::NOT_INITIALIZED, "Device not initialized");
    }

    if (!data || size == 0) {
        return Result<Transaction>::failure(SPIError::ALLOCATION_FAILED, "Invalid data or size");
    }

    // Check if an async operation is already in progress
    if (pImpl->async_state.active) {
        return Result<Transaction>::failure(SPIError::BUSY, "Another async operation is in progress");
    }

    // Acquire DMA buffer
    DMABuffer buffer = acquireBuffer(size);
    if (!buffer.ok()) {
        FL_WARN("SPI Device: Failed to acquire DMA buffer for async write");
        return Result<Transaction>::failure(buffer.error(), "Failed to acquire DMA buffer");
    }

    // Copy data to DMA buffer
    fl::span<uint8_t> buf_span = buffer.data();
    if (buf_span.size() < size) {
        FL_WARN("SPI Device: Buffer size mismatch");
        return Result<Transaction>::failure(SPIError::BUFFER_TOO_LARGE, "Buffer size mismatch");
    }

    for (size_t i = 0; i < size; i++) {
        buf_span[i] = data[i];
    }

    // Start async transmission
    fl::optional<fl::Error> tx_result = transmit(buffer, true);  // true = async
    if (tx_result) {  // If error is present
        FL_WARN("SPI Device: Failed to start async transmission");
        return Result<Transaction>::failure(SPIError::NOT_SUPPORTED, tx_result->message.c_str());
    }

    // Store async state
    pImpl->async_state.active = true;
    pImpl->async_state.tx_buffer = data;
    pImpl->async_state.rx_buffer = nullptr;
    pImpl->async_state.size = size;
    pImpl->async_state.start_time = fl::millis();

    // Create Transaction object with proper initialization
    Transaction txn;
    txn.pImpl = fl::make_unique<Transaction::Impl>(this);
    txn.pImpl->completed = false;  // Will complete when hardware finishes

    FL_LOG_SPI("SPI Device: Async write started (" << size << " bytes)");
    return Result<Transaction>::success(fl::move(txn));
}

// Commented out - methods not declared in header
// Result<Transaction> Device::readAsync(uint8_t* buffer, size_t size) {
//     FL_UNUSED(buffer);
//     FL_UNUSED(size);
//
//     FL_WARN("SPI Device: readAsync() not supported - FastLED uses TX-only SPI for LED strips");
//     return Result<Transaction>::failure(SPIError::NOT_SUPPORTED,
//         "Async read operations not supported - LED strips are TX-only (no MISO/readback)");
// }
//
// Result<Transaction> Device::transferAsync(const uint8_t* tx_data, uint8_t* rx_buffer, size_t size) {
//     FL_UNUSED(tx_data);
//     FL_UNUSED(rx_buffer);
//     FL_UNUSED(size);
//
//     FL_WARN("SPI Device: transferAsync() not supported - FastLED uses TX-only SPI. Use writeAsync() instead.");
//     return Result<Transaction>::failure(SPIError::NOT_SUPPORTED,
//         "Async full-duplex transfer not supported - LED strips are TX-only. Use writeAsync() for transmission.");
// }

DMABuffer Device::acquireBuffer(size_t size) {
    if (!isReady()) {
        return DMABuffer(SPIError::NOT_INITIALIZED);
    }

    // Get hardware controller
    if (!pImpl->hw_backend) {
        FL_WARN("SPI Device: No hardware controller available");
        return DMABuffer(SPIError::NOT_INITIALIZED);
    }

    // Use polymorphic interface (works for SpiHw1/2/4/8)
    SpiHwBase* hw = pImpl->hw_backend.get();

    // Always acquire a fresh buffer from hardware
    // Note: The hardware controller may internally cache/reuse buffers
    DMABuffer buffer = hw->acquireDMABuffer(size);

    if (!buffer.ok()) {
        FL_WARN("SPI Device: Failed to acquire DMA buffer from hardware");
    } else {
        FL_LOG_SPI("SPI Device: Acquired DMA buffer (" << size << " bytes)");
    }

    return buffer;
}

fl::optional<fl::Error> Device::transmit(DMABuffer& buffer, bool async) {
    if (!isReady()) {
        return fl::Error("Device not initialized");
    }

    if (!buffer.ok()) {
        return fl::Error("Invalid buffer");
    }

    // Get hardware controller
    if (!pImpl->hw_backend) {
        FL_WARN("SPI Device: No hardware controller available");
        return fl::Error("No hardware controller");
    }

    // Use polymorphic interface (works for SpiHw1/2/4/8)
    SpiHwBase* hw = pImpl->hw_backend.get();

    // Start transmission
    TransmitMode mode = async ? TransmitMode::ASYNC : TransmitMode::SYNC;
    bool success = hw->transmit(mode);

    if (!success) {
        FL_WARN("SPI Device: Transmission failed");
        return fl::Error("Transmission failed");
    }

    // If blocking mode, wait for completion
    if (!async) {
        if (!hw->waitComplete()) {
            FL_WARN("SPI Device: Wait for completion failed");
            return fl::Error("Wait for completion failed");
        }
    }

    FL_LOG_SPI("SPI Device: Transmission started (" << (async ? "async" : "blocking") << ")");
    return fl::nullopt;
}

bool Device::waitComplete(uint32_t timeout_ms) {
    if (!isReady()) {
        return false;
    }

    // Get hardware controller
    if (!pImpl->hw_backend) {
        FL_WARN("SPI Device: No hardware controller available");
        return false;
    }

    // Use polymorphic interface (works for SpiHw1/2/4/8)
    SpiHwBase* hw = pImpl->hw_backend.get();

    return hw->waitComplete(timeout_ms);
}

bool Device::isBusy() const {
    if (!isReady()) {
        return false;
    }

    // Get hardware controller
    if (!pImpl->hw_backend) {
        return false;
    }

    // Use polymorphic interface (works for SpiHw1/2/4/8)
    SpiHwBase* hw = pImpl->hw_backend.get();

    return hw->isBusy();
}

fl::optional<fl::Error> Device::setClockSpeed(uint32_t speed_hz) {
    if (!pImpl) {
        return fl::Error("Device not initialized");
    }

    // Update the configuration
    pImpl->config.clock_speed_hz = speed_hz;

    // Note: Runtime clock speed updates are not currently supported by the hardware layer.
    // The new speed will take effect on the next begin() call.
    // To apply immediately, call end() followed by begin().

    if (pImpl->initialized) {
        FL_LOG_SPI("SPI Device: Clock speed updated to " << speed_hz
               << " Hz (will take effect on next begin())");
    } else {
        FL_LOG_SPI("SPI Device: Clock speed set to " << speed_hz << " Hz");
    }

    return fl::nullopt;
}

const Config& Device::getConfig() const {
    // Note: Caller must ensure Device is valid
    return pImpl->config;
}

// ============================================================================
// Transaction Implementation (Stub)
// ============================================================================

Transaction::Transaction() : pImpl(nullptr) {}

Transaction::Transaction(Transaction&& other) noexcept
    : pImpl(fl::move(other.pImpl)) {
}

Transaction& Transaction::operator=(Transaction&& other) noexcept {
    if (this != &other) {
        pImpl = fl::move(other.pImpl);
    }
    return *this;
}

Transaction::~Transaction() {
    if (pImpl) {
        // Auto-wait for completion if not already done
        if (!pImpl->completed) {
            wait();
        }
    }
}

bool Transaction::wait(uint32_t timeout_ms) {
    if (!pImpl) {
        return true;  // Already completed (or invalid)
    }

    if (pImpl->completed) {
        return true;  // Already completed
    }

    if (pImpl->cancelled) {
        pImpl->completed = true;
        return false;  // Was cancelled
    }

    // Check if device is still valid
    if (!pImpl->device) {
        pImpl->completed = true;
        pImpl->result = fl::Error("Device pointer is null");
        return false;
    }

    if (!pImpl->device->isReady()) {
        pImpl->completed = true;
        pImpl->result = fl::Error("Device not ready");
        return false;
    }

    // Wait for the hardware to complete
    uint32_t start_time = fl::millis();
    bool success = pImpl->device->waitComplete(timeout_ms);

    if (success) {
        // Clear async state in device
        if (pImpl->device->pImpl) {
            pImpl->device->pImpl->async_state.active = false;
        }
        pImpl->completed = true;
        pImpl->result = fl::nullopt;

        uint32_t elapsed = fl::millis() - start_time;
        FL_LOG_SPI("Transaction: Completed successfully (waited " << elapsed << "ms)");
        return true;
    } else {
        // Timeout occurred
        pImpl->completed = true;
        pImpl->result = fl::Error("Transaction timeout");
        FL_WARN("Transaction: Timeout after " << timeout_ms << "ms");
        return false;
    }
}

bool Transaction::isDone() const {
    return pImpl ? pImpl->completed : true;
}

bool Transaction::isPending() const {
    return pImpl ? !pImpl->completed : false;
}

bool Transaction::cancel() {
    if (!pImpl || pImpl->completed) {
        return false;  // Already completed or invalid
    }

    // Note: Cancellation of in-progress DMA transfers is platform-specific
    // and not always supported. For now, we mark it as cancelled and
    // the next wait() call will handle it.

    pImpl->cancelled = true;
    pImpl->completed = true;
    pImpl->result = fl::Error("Transaction cancelled");

    // Clear async state in device
    if (pImpl->device && pImpl->device->pImpl) {
        pImpl->device->pImpl->async_state.active = false;
    }

    FL_LOG_SPI("Transaction: Cancelled");
    return true;
}

fl::optional<fl::Error> Transaction::getResult() const {
    if (!pImpl) {
        return fl::Error("Invalid transaction");
    }
    return pImpl->result;
}

} // namespace spi
} // namespace fl
