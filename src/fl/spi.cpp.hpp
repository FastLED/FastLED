#include "fl/spi.h"
#include "fl/spi/multi_lane_device.h"
#include "fl/warn.h"
#include "fl/dbg.h"

namespace fl {

// ============================================================================
// Spi Constructors
// ============================================================================

Spi::Spi(int clock_pin, fl::span<const int> data_pins,
         spi_output_mode_t output_mode,
         uint32_t clock_speed_hz)
    : Spi(SpiConfig(clock_pin, data_pins, clock_speed_hz, output_mode, 0)) {
}

Spi::Spi(const SpiConfig& config)
    : is_ok(false), error_code(SPIError::NOT_INITIALIZED) {

    // Validate number of lanes
    size_t num_lanes = config.data_pins.size();
    if (num_lanes < 1 || num_lanes > 8) {
        FL_WARN("fl::Spi: Invalid number of data pins (" << num_lanes << "), must be 1-8");
        error_code = SPIError::NOT_INITIALIZED;
        return;
    }

    // Convert SpiConfig to MultiLaneDevice::Config
    spi::MultiLaneDevice::Config ml_config;
    ml_config.clock_pin = static_cast<uint8_t>(config.clock_pin);
    ml_config.data_pins.resize(config.data_pins.size());
    for (size_t i = 0; i < config.data_pins.size(); i++) {
        ml_config.data_pins[i] = static_cast<uint8_t>(config.data_pins[i]);
    }
    ml_config.clock_speed_hz = config.clock_speed_hz;
    ml_config.mode = config.spi_mode;

    // Create device
    device = fl::make_unique<spi::MultiLaneDevice>(ml_config);
    FL_DBG("fl::Spi: Created MultiLaneDevice with " << num_lanes << " lane(s)");

    // Initialize device
    auto begin_result = device->begin();
    if (begin_result) {
        // begin() returned an error (optional has a value)
        FL_WARN("fl::Spi: begin() failed");
        error_code = SPIError::NOT_INITIALIZED;
        device.reset();
        return;
    }

    is_ok = true;
}

Spi::Spi(Spi&& other) noexcept
    : device(fl::move(other.device))
    , is_ok(other.is_ok)
    , error_code(other.error_code) {
    other.is_ok = false;
}

Spi& Spi::operator=(Spi&& other) noexcept {
    if (this != &other) {
        device = fl::move(other.device);
        is_ok = other.is_ok;
        error_code = other.error_code;
        other.is_ok = false;
    }
    return *this;
}

bool Spi::wait(uint32_t timeout_ms) {
    if (!device) {
        return false;
    }
    return device->waitComplete(timeout_ms);
}

} // namespace fl
