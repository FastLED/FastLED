#include "fl/spi/multi_lane_device.h"
#include "fl/spi/device_impl_base.h"
#include "fl/warn.h"
#include "fl/dbg.h"
#include "fl/str.h"
#include "platforms/shared/spi_hw_1.h"
#include "platforms/shared/spi_hw_2.h"
#include "platforms/shared/spi_hw_4.h"
#include "platforms/shared/spi_hw_8.h"
#include "platforms/shared/spi_transposer.h"

namespace fl {
namespace spi {

// ============================================================================
// Implementation Details (pImpl pattern)
// ============================================================================

struct MultiLaneDevice::Impl : public DeviceImplBase {
    Config config;
    fl::vector<Lane> lanes;
    uint8_t backend_type;  // 1, 2, 4, or 8 (number of lanes supported by backend)

    Impl(const Config& cfg)
        : DeviceImplBase()
        , config(cfg)
        , backend_type(0) {

        // Create Lane objects
        size_t num_lanes = config.data_pins.size();
        for (size_t i = 0; i < num_lanes; i++) {
            lanes.push_back(Lane(i, nullptr));  // Parent set later
        }
    }

    ~Impl() {
        if (initialized && backend) {
            releaseBackend();
        }
    }

    void releaseBackend() {
        if (!backend) return;

        // Use polymorphic interface - no casting needed!
        backend->end();

        clearBackend();  // Use base class method
    }
};

// ============================================================================
// MultiLaneDevice Implementation
// ============================================================================

MultiLaneDevice::MultiLaneDevice(const Config& config)
    : pImpl(fl::make_unique<Impl>(config)) {

    // Validate configuration
    size_t num_lanes = config.data_pins.size();
    if (num_lanes < 1 || num_lanes > 8) {
        FL_WARN("MultiLaneDevice: Invalid number of data pins (" << num_lanes
                << "), must be 1-8");
    }

    FL_DBG("MultiLaneDevice: Created with " << num_lanes << " lane(s)");
}

MultiLaneDevice::~MultiLaneDevice() {
    if (pImpl && pImpl->initialized) {
        end();
    }
}

fl::optional<fl::Error> MultiLaneDevice::begin() {
    if (!pImpl) {
        return fl::Error("Device not initialized");
    }

    if (pImpl->initialized) {
        // Already initialized - idempotent
        return fl::nullopt;
    }

    size_t num_lanes = pImpl->config.data_pins.size();

    // Validate lane count
    if (num_lanes < 1 || num_lanes > 8) {
        return fl::Error("Invalid number of lanes (must be 1-8)");
    }

    // Auto-select appropriate hardware backend based on lane count
    if (num_lanes == 1) {
        // Try Single-SPI (SpiHw1)
        const auto& controllers = SpiHw1::getAll();
        if (controllers.empty()) {
            FL_WARN("MultiLaneDevice: No Single-SPI hardware available");
            return fl::Error("Single-SPI hardware not available");
        }

        // Find first available controller
        fl::shared_ptr<SpiHw1> hw;
        for (const auto& ctrl : controllers) {
            if (!ctrl->isInitialized()) {
                hw = ctrl;
                break;
            }
        }

        if (!hw) {
            FL_WARN("MultiLaneDevice: All Single-SPI controllers in use");
            return fl::Error("All Single-SPI controllers already in use");
        }

        // Configure Single-SPI
        SpiHw1::Config hw_config;
        hw_config.bus_num = static_cast<uint8_t>(hw->getBusId());
        hw_config.clock_speed_hz = pImpl->config.clock_speed_hz;
        hw_config.clock_pin = pImpl->config.clock_pin;
        hw_config.data_pin = pImpl->config.data_pins[0];

        if (!hw->begin(hw_config)) {
            FL_WARN("MultiLaneDevice: Failed to initialize Single-SPI hardware");
            return fl::Error("Failed to initialize Single-SPI hardware");
        }

        pImpl->backend = hw;
        pImpl->backend_type = 1;
        FL_DBG("MultiLaneDevice: Initialized Single-SPI (" << hw->getName() << ")");

    } else if (num_lanes == 2) {
        // Try Dual-SPI (SpiHw2)
        const auto& controllers = SpiHw2::getAll();
        if (controllers.empty()) {
            FL_WARN("MultiLaneDevice: No Dual-SPI hardware available");
            return fl::Error("Dual-SPI hardware not available");
        }

        // Find first available controller
        fl::shared_ptr<SpiHw2> hw;
        for (const auto& ctrl : controllers) {
            if (!ctrl->isInitialized()) {
                hw = ctrl;
                break;
            }
        }

        if (!hw) {
            FL_WARN("MultiLaneDevice: All Dual-SPI controllers in use");
            return fl::Error("All Dual-SPI controllers already in use");
        }

        // Configure Dual-SPI
        SpiHw2::Config hw_config;
        hw_config.bus_num = static_cast<uint8_t>(hw->getBusId());
        hw_config.clock_speed_hz = pImpl->config.clock_speed_hz;
        hw_config.clock_pin = pImpl->config.clock_pin;
        hw_config.data0_pin = pImpl->config.data_pins[0];
        hw_config.data1_pin = pImpl->config.data_pins[1];

        if (!hw->begin(hw_config)) {
            FL_WARN("MultiLaneDevice: Failed to initialize Dual-SPI hardware");
            return fl::Error("Failed to initialize Dual-SPI hardware");
        }

        pImpl->backend = hw;
        pImpl->backend_type = 2;
        FL_DBG("MultiLaneDevice: Initialized Dual-SPI (" << hw->getName() << ")");

    } else if (num_lanes >= 3 && num_lanes <= 4) {
        // Try Quad-SPI (SpiHw4)
        const auto& controllers = SpiHw4::getAll();
        if (controllers.empty()) {
            FL_WARN("MultiLaneDevice: No Quad-SPI hardware available");
            return fl::Error("Quad-SPI hardware not available");
        }

        // Find first available controller
        fl::shared_ptr<SpiHw4> hw;
        for (const auto& ctrl : controllers) {
            if (!ctrl->isInitialized()) {
                hw = ctrl;
                break;
            }
        }

        if (!hw) {
            FL_WARN("MultiLaneDevice: All Quad-SPI controllers in use");
            return fl::Error("All Quad-SPI controllers already in use");
        }

        // Configure Quad-SPI
        SpiHw4::Config hw_config;
        hw_config.bus_num = static_cast<uint8_t>(hw->getBusId());
        hw_config.clock_speed_hz = pImpl->config.clock_speed_hz;
        hw_config.clock_pin = pImpl->config.clock_pin;
        hw_config.data0_pin = pImpl->config.data_pins[0];
        hw_config.data1_pin = (num_lanes > 1) ? pImpl->config.data_pins[1] : -1;
        hw_config.data2_pin = (num_lanes > 2) ? pImpl->config.data_pins[2] : -1;
        hw_config.data3_pin = (num_lanes > 3) ? pImpl->config.data_pins[3] : -1;

        if (!hw->begin(hw_config)) {
            FL_WARN("MultiLaneDevice: Failed to initialize Quad-SPI hardware");
            return fl::Error("Failed to initialize Quad-SPI hardware");
        }

        pImpl->backend = hw;
        pImpl->backend_type = 4;
        FL_DBG("MultiLaneDevice: Initialized Quad-SPI (" << hw->getName() << ")");

    } else if (num_lanes >= 5 && num_lanes <= 8) {
        // Try Octal-SPI (SpiHw8)
        const auto& controllers = SpiHw8::getAll();
        if (controllers.empty()) {
            FL_WARN("MultiLaneDevice: No Octal-SPI hardware available");
            return fl::Error("Octal-SPI hardware not available");
        }

        // Find first available controller
        fl::shared_ptr<SpiHw8> hw;
        for (const auto& ctrl : controllers) {
            if (!ctrl->isInitialized()) {
                hw = ctrl;
                break;
            }
        }

        if (!hw) {
            FL_WARN("MultiLaneDevice: All Octal-SPI controllers in use");
            return fl::Error("All Octal-SPI controllers already in use");
        }

        // Configure Octal-SPI
        SpiHw8::Config hw_config;
        hw_config.bus_num = static_cast<uint8_t>(hw->getBusId());
        hw_config.clock_speed_hz = pImpl->config.clock_speed_hz;
        hw_config.clock_pin = pImpl->config.clock_pin;
        hw_config.data0_pin = pImpl->config.data_pins[0];
        hw_config.data1_pin = (num_lanes > 1) ? pImpl->config.data_pins[1] : -1;
        hw_config.data2_pin = (num_lanes > 2) ? pImpl->config.data_pins[2] : -1;
        hw_config.data3_pin = (num_lanes > 3) ? pImpl->config.data_pins[3] : -1;
        hw_config.data4_pin = (num_lanes > 4) ? pImpl->config.data_pins[4] : -1;
        hw_config.data5_pin = (num_lanes > 5) ? pImpl->config.data_pins[5] : -1;
        hw_config.data6_pin = (num_lanes > 6) ? pImpl->config.data_pins[6] : -1;
        hw_config.data7_pin = (num_lanes > 7) ? pImpl->config.data_pins[7] : -1;

        if (!hw->begin(hw_config)) {
            FL_WARN("MultiLaneDevice: Failed to initialize Octal-SPI hardware");
            return fl::Error("Failed to initialize Octal-SPI hardware");
        }

        pImpl->backend = hw;
        pImpl->backend_type = 8;
        FL_DBG("MultiLaneDevice: Initialized Octal-SPI (" << hw->getName() << ")");
    }

    pImpl->initialized = true;
    return fl::nullopt;
}

void MultiLaneDevice::end() {
    if (!pImpl || !pImpl->initialized) {
        return;
    }

    // Wait for pending operations
    waitComplete();

    // Release hardware backend
    pImpl->releaseBackend();

    // Clear lane buffers
    for (auto& lane : pImpl->lanes) {
        lane.clear();
    }

    FL_DBG("MultiLaneDevice: Shutdown complete");
}

bool MultiLaneDevice::isReady() const {
    return pImpl && pImpl->isReady();
}

Lane& MultiLaneDevice::lane(size_t lane_id) {
    if (!pImpl || lane_id >= pImpl->lanes.size()) {
        FL_WARN("MultiLaneDevice: Invalid lane ID " << lane_id);
        // Return first lane as fallback (avoid crash)
        static Lane dummy_lane(0, nullptr);
        return dummy_lane;
    }
    return pImpl->lanes[lane_id];
}

size_t MultiLaneDevice::numLanes() const {
    return pImpl ? pImpl->lanes.size() : 0;
}

Result<void> MultiLaneDevice::flush() {
    if (!isReady()) {
        return Result<void>::failure(SPIError::NOT_INITIALIZED,
            "Device not initialized");
    }

    // Find lane sizes and validate all non-empty lanes have the same size
    size_t expected_size = 0;
    bool found_first = false;

    for (size_t i = 0; i < pImpl->lanes.size(); i++) {
        size_t lane_size = pImpl->lanes[i].bufferSize();

        if (lane_size > 0) {
            if (!found_first) {
                // First non-empty lane sets the expected size
                expected_size = lane_size;
                found_first = true;
            } else if (lane_size != expected_size) {
                // Size mismatch detected
                FL_WARN("MultiLaneDevice: Lane size mismatch - expected " << expected_size
                        << " bytes (lane 0), but lane " << i << " has " << lane_size << " bytes");
                return Result<void>::failure(SPIError::INVALID_PARAMETER,
                    "Lane size mismatch: all lanes must have identical sizes");
            }
        }
    }

    if (expected_size == 0) {
        FL_WARN("MultiLaneDevice: No data to flush (all lanes empty)");
        return Result<void>::failure(SPIError::ALLOCATION_FAILED,
            "No data to transmit");
    }

    // Use expected_size for DMA buffer allocation (all lanes now guaranteed same size)
    size_t max_size = expected_size;

    // Acquire DMA buffer from hardware backend - use polymorphic interface
    DMABuffer dma_buffer = pImpl->backend->acquireDMABuffer(max_size);

    if (!dma_buffer.ok()) {
        FL_WARN("MultiLaneDevice: Failed to acquire DMA buffer");
        return Result<void>::failure(dma_buffer.error(),
            "Failed to acquire DMA buffer");
    }

    // Transpose lanes into DMA buffer (or copy for single lane)
    const char* error = nullptr;
    bool transpose_ok = false;

    if (pImpl->backend_type == 1) {
        // Single lane - no transposition needed, just copy data directly
        if (pImpl->lanes.size() > 0) {
            fl::span<const uint8_t> lane_data = pImpl->lanes[0].data();
            fl::span<uint8_t> dma_data = dma_buffer.data();

            // Verify sizes match (DMA buffer should be exactly the size we requested)
            if (lane_data.size() != dma_data.size()) {
                FL_WARN("MultiLaneDevice: DMA buffer size mismatch - expected " << lane_data.size()
                        << " bytes, got " << dma_data.size() << " bytes");
                error = "DMA buffer size mismatch";
                transpose_ok = false;
            } else {
                // Copy lane data to DMA buffer
                for (size_t i = 0; i < lane_data.size(); i++) {
                    dma_data[i] = lane_data[i];
                }
                transpose_ok = true;
            }
        } else {
            error = "No lanes configured";
            transpose_ok = false;
        }

    } else if (pImpl->backend_type == 2) {
        // Dual-SPI transposition
        fl::optional<SPITransposer::LaneData> lane0, lane1;
        if (pImpl->lanes.size() > 0) {
            lane0 = SPITransposer::LaneData{
                pImpl->lanes[0].data(),
                fl::span<const uint8_t>()  // No padding
            };
        }
        if (pImpl->lanes.size() > 1) {
            lane1 = SPITransposer::LaneData{
                pImpl->lanes[1].data(),
                fl::span<const uint8_t>()  // No padding
            };
        }

        transpose_ok = SPITransposer::transpose2(lane0, lane1, dma_buffer.data(), &error);

    } else if (pImpl->backend_type == 4) {
        // Quad-SPI transposition
        fl::optional<SPITransposer::LaneData> lanes[4];
        for (size_t i = 0; i < pImpl->lanes.size() && i < 4; i++) {
            lanes[i] = SPITransposer::LaneData{
                pImpl->lanes[i].data(),
                fl::span<const uint8_t>()  // No padding
            };
        }

        transpose_ok = SPITransposer::transpose4(lanes[0], lanes[1], lanes[2], lanes[3],
                                                  dma_buffer.data(), &error);

    } else if (pImpl->backend_type == 8) {
        // Octal-SPI transposition
        fl::optional<SPITransposer::LaneData> lanes[8];
        for (size_t i = 0; i < pImpl->lanes.size() && i < 8; i++) {
            lanes[i] = SPITransposer::LaneData{
                pImpl->lanes[i].data(),
                fl::span<const uint8_t>()  // No padding
            };
        }

        transpose_ok = SPITransposer::transpose8(lanes, dma_buffer.data(), &error);
    }

    if (!transpose_ok) {
        FL_WARN("MultiLaneDevice: Transposition failed - " << (error ? error : "unknown error"));
        return Result<void>::failure(SPIError::ALLOCATION_FAILED,
            error ? error : "Transposition failed");
    }

    // Transmit via hardware backend - use polymorphic interface
    bool transmit_ok = pImpl->backend->transmit(TransmitMode::ASYNC);

    if (!transmit_ok) {
        FL_WARN("MultiLaneDevice: Hardware transmit failed");
        return Result<void>::failure(SPIError::BUSY,
            "Hardware transmit failed");
    }

    // Clear lane buffers after starting transmission
    for (auto& lane : pImpl->lanes) {
        lane.clear();
    }

    FL_DBG("MultiLaneDevice: Flushed " << pImpl->lanes.size() << " lanes ("
           << max_size << " bytes per lane)");

    // Success - transmission started asynchronously
    // User must call waitComplete() manually to block until transmission completes
    return Result<void>::success();
}

bool MultiLaneDevice::waitComplete(uint32_t timeout_ms) {
    if (!isReady()) {
        return false;
    }

    // Use polymorphic interface - no casting needed!
    return pImpl->backend->waitComplete(timeout_ms);
}

bool MultiLaneDevice::isBusy() const {
    if (!isReady()) {
        return false;
    }

    // Use polymorphic interface - no casting needed!
    return pImpl->backend->isBusy();
}

WriteResult MultiLaneDevice::writeImpl(fl::span<const fl::span<const uint8_t>> lane_data) {
    if (!isReady()) {
        FL_WARN("MultiLaneDevice: Not ready for write");
        return WriteResult("Device not ready");
    }

    if (lane_data.size() > pImpl->lanes.size()) {
        FL_WARN("MultiLaneDevice: Too many lanes provided (" << lane_data.size()
                << " > " << pImpl->lanes.size() << ")");
        return WriteResult("Too many lanes provided");
    }

    // Validate that all lanes have the same size (required for reliable transposition)
    if (lane_data.size() > 1) {
        size_t first_size = lane_data[0].size();
        for (size_t i = 1; i < lane_data.size(); i++) {
            if (lane_data[i].size() != first_size) {
                FL_WARN("MultiLaneDevice: Lane size mismatch - lane 0 has " << first_size
                        << " bytes, lane " << i << " has " << lane_data[i].size() << " bytes");
                return WriteResult("Lane size mismatch: all lanes must have identical sizes");
            }
        }
    }

    // Wait for previous transmission to complete
    waitComplete();

    // Write all lane data (can be optimized later with batch operations)
    for (size_t i = 0; i < lane_data.size(); i++) {
        pImpl->lanes[i].write(lane_data[i].data(), lane_data[i].size());
    }

    // Start hardware transmission (async)
    auto flush_result = flush();
    if (!flush_result.ok()) {
        FL_WARN("MultiLaneDevice: Flush failed");
        return WriteResult("Flush failed");
    }

    FL_DBG("MultiLaneDevice: Wrote " << lane_data.size() << " lanes atomically (async)");

    // Return success - use device->wait() to block until complete
    return WriteResult();
}

const MultiLaneDevice::Config& MultiLaneDevice::getConfig() const {
    static const Config empty_config;
    return pImpl ? pImpl->config : empty_config;
}

} // namespace spi
} // namespace fl
