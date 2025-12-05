#include "rmt_rx_channel.h"

#ifdef ESP32
#if FASTLED_RMT5

#include "fl/dbg.h"
#include "fl/warn.h"

FL_EXTERN_C_BEGIN
#include "esp_err.h"
FL_EXTERN_C_END

namespace fl {
namespace esp32 {

// Constructor
RmtRxChannel::RmtRxChannel(gpio_num_t pin, uint32_t resolution_hz)
    : channel_(nullptr)
    , pin_(pin)
    , resolution_hz_(resolution_hz)
    , receive_done_(false)
    , symbols_received_(0)
{
    FL_DBG("RmtRxChannel constructed: pin=" << static_cast<int>(pin_)
           << " resolution=" << resolution_hz_ << "Hz");
}

// Destructor
RmtRxChannel::~RmtRxChannel() {
    if (channel_) {
        FL_DBG("Deleting RMT RX channel");
        rmt_del_channel(channel_);
        channel_ = nullptr;
    }
}

// Initialize RX channel
bool RmtRxChannel::begin() {
    if (channel_) {
        FL_WARN("RX channel already initialized");
        return true;
    }

    // Configure RX channel
    rmt_rx_channel_config_t rx_config = {};
    rx_config.gpio_num = pin_;
    rx_config.clk_src = RMT_CLK_SRC_DEFAULT;
    rx_config.resolution_hz = resolution_hz_;
    rx_config.mem_block_symbols = 64;  // Use 64 symbols per memory block

    // Additional flags
    rx_config.flags.invert_in = false;  // No signal inversion
    rx_config.flags.with_dma = false;   // Start with non-DMA (universal compatibility)
    rx_config.flags.io_loop_back = true;  // Enable internal GPIO loopback (same pin TX+RX)

    // Create RX channel
    esp_err_t err = rmt_new_rx_channel(&rx_config, &channel_);
    if (err != ESP_OK) {
        FL_WARN("Failed to create RX channel: " << static_cast<int>(err));
        return false;
    }

    FL_DBG("RX channel created successfully");

    // Register ISR callback
    rmt_rx_event_callbacks_t callbacks = {};
    callbacks.on_recv_done = rxDoneCallback;

    err = rmt_rx_register_event_callbacks(channel_, &callbacks, this);
    if (err != ESP_OK) {
        FL_WARN("Failed to register RX callbacks: " << static_cast<int>(err));
        rmt_del_channel(channel_);
        channel_ = nullptr;
        return false;
    }

    FL_DBG("RX callbacks registered successfully");

    // Enable RX channel
    err = rmt_enable(channel_);
    if (err != ESP_OK) {
        FL_WARN("Failed to enable RX channel: " << static_cast<int>(err));
        rmt_del_channel(channel_);
        channel_ = nullptr;
        return false;
    }

    FL_DBG("RX channel enabled");
    return true;
}

// Start receiving symbols
bool RmtRxChannel::startReceive(rmt_symbol_word_t* buffer, size_t buffer_size) {
    if (!channel_) {
        FL_WARN("RX channel not initialized (call begin() first)");
        return false;
    }

    if (!buffer || buffer_size == 0) {
        FL_WARN("Invalid buffer parameters");
        return false;
    }

    // Reset state
    receive_done_ = false;
    symbols_received_ = 0;

    // Configure receive parameters
    rmt_receive_config_t rx_params = {};
    rx_params.signal_range_min_ns = 100;      // Min pulse width: 100ns
    rx_params.signal_range_max_ns = 800000;    // Max pulse width: 800µs (below 819µs HW limit)

    // Start receiving
    esp_err_t err = rmt_receive(channel_, buffer, buffer_size * sizeof(rmt_symbol_word_t), &rx_params);
    if (err != ESP_OK) {
        FL_WARN("Failed to start RX receive: " << static_cast<int>(err));
        return false;
    }

    FL_DBG("RX receive started (buffer size: " << buffer_size << " symbols)");
    return true;
}

// Reset receive state
void RmtRxChannel::reset() {
    receive_done_ = false;
    symbols_received_ = 0;
    FL_DBG("RX channel reset");
}

// Re-enable RX channel (needed after receive completes/times out)
bool RmtRxChannel::enable() {
    if (!channel_) {
        FL_WARN("RX channel not initialized (call begin() first)");
        return false;
    }

    // Disable first to avoid ESP_ERR_INVALID_STATE (259) if already enabled
    // This is safe to call even if already disabled
    esp_err_t err = rmt_disable(channel_);
    if (err != ESP_OK) {
        FL_DBG("rmt_disable returned: " << static_cast<int>(err) << " (ignoring)");
        // Continue anyway - may already be disabled
    }

    // Now enable the channel
    err = rmt_enable(channel_);
    if (err != ESP_OK) {
        FL_WARN("Failed to re-enable RX channel: " << static_cast<int>(err));
        return false;
    }

    FL_DBG("RX channel re-enabled");
    return true;
}

// ISR callback for receive completion
bool IRAM_ATTR RmtRxChannel::rxDoneCallback(rmt_channel_handle_t channel,
                                             const rmt_rx_done_event_data_t* data,
                                             void* user_data) {
    RmtRxChannel* self = static_cast<RmtRxChannel*>(user_data);
    if (!self) {
        return false;
    }

    // Update receive state
    self->symbols_received_ = data->num_symbols;
    self->receive_done_ = true;

    // No higher-priority task awakened
    return false;
}

} // namespace esp32
} // namespace fl

#endif // FASTLED_RMT5
#endif // ESP32
