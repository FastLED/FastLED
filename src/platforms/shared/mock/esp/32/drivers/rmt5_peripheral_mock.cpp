/// @file rmt5_peripheral_mock.cpp
/// @brief Mock RMT5 peripheral implementation for unit testing

// This mock is only for host testing
// Compile for stub platform testing OR non-Arduino host platforms
#include "platforms/is_platform.h"
#if defined(FASTLED_STUB_IMPL) || (!defined(ARDUINO) && (defined(FL_IS_LINUX) || defined(FL_IS_APPLE) || defined(FL_IS_WIN)))

#include "rmt5_peripheral_mock.h"
#include "fl/warn.h"
#include "fl/dbg.h"
#include "fl/stl/bit_cast.h"
#include "fl/stl/cstring.h"
#include "fl/singleton.h"
#include "fl/stl/unordered_map.h"

#ifdef FL_IS_WIN
#include <malloc.h>  // ok include - For _aligned_malloc/_aligned_free on Windows
#else
#include <stdlib.h>  // For aligned_alloc on POSIX
#endif

#ifdef ARDUINO
#include "fl/arduino.h"
#else
#include "platforms/stub/time_stub.h"  // For fl::micros() on host tests
#endif

namespace fl {
namespace detail {

//=============================================================================
// Mock Channel and Encoder Data
//=============================================================================

struct MockChannel {
    int id;
    Rmt5ChannelConfig config;
    bool enabled;
    Rmt5TxDoneCallback callback;
    void* user_ctx;

    MockChannel()
        : id(0), config(), enabled(false), callback(nullptr), user_ctx(nullptr) {}
};

struct MockEncoder {
    int id;
    ChipsetTiming timing;
    u32 resolution_hz;

    MockEncoder()
        : id(0), timing(), resolution_hz(0) {}
};

//=============================================================================
// Implementation Class (internal)
//=============================================================================

/// @brief Internal implementation of Rmt5PeripheralMock
///
/// This class contains all the actual implementation details.
/// It simulates RMT5 hardware behavior for unit testing.
class Rmt5PeripheralMockImpl : public Rmt5PeripheralMock {
public:
    //=========================================================================
    // Lifecycle
    //=========================================================================

    Rmt5PeripheralMockImpl();
    ~Rmt5PeripheralMockImpl() override;

    //=========================================================================
    // IRMT5Peripheral Interface Implementation
    //=========================================================================

    bool createTxChannel(const Rmt5ChannelConfig& config,
                         void** out_handle) override;
    bool deleteChannel(void* channel_handle) override;
    bool enableChannel(void* channel_handle) override;
    bool disableChannel(void* channel_handle) override;
    bool transmit(void* channel_handle, void* encoder_handle,
                  const u8* buffer, size_t buffer_size) override;
    bool waitAllDone(void* channel_handle, u32 timeout_ms) override;
    void* createEncoder(const ChipsetTiming& timing,
                        u32 resolution_hz) override;
    void deleteEncoder(void* encoder_handle) override;
    bool resetEncoder(void* encoder_handle) override;
    bool registerTxCallback(void* channel_handle,
                            Rmt5TxDoneCallback callback,
                            void* user_ctx) override;
    void configureLogging() override;
    bool syncCache(void* buffer, size_t size) override;
    u8* allocateDmaBuffer(size_t size) override;
    void freeDmaBuffer(u8* buffer) override;

    //=========================================================================
    // Mock-Specific API
    //=========================================================================

    void simulateTransmitDone(void* channel_handle) override;
    void setTransmitFailure(bool should_fail) override;
    const fl::vector<TransmissionRecord>& getTransmissionHistory() const override;
    void clearTransmissionHistory() override;
    fl::span<const u8> getLastTransmissionData() const override;
    size_t getChannelCount() const override;
    size_t getEncoderCount() const override;
    size_t getTransmissionCount() const override;
    bool isChannelEnabled(void* channel_handle) const override;
    void reset() override;

private:
    //=========================================================================
    // Internal State
    //=========================================================================

    // Channel and encoder management (store pointers for stable references)
    fl::unordered_map<int, MockChannel*> mChannels;
    fl::unordered_map<int, MockEncoder*> mEncoders;
    int mNextChannelId;
    int mNextEncoderId;

    // Simulation settings
    bool mShouldFailTransmit;

    // Waveform capture
    fl::vector<TransmissionRecord> mHistory;
    size_t mTransmissionCount;

    // Helper methods
    MockChannel* findChannel(void* handle);
    const MockChannel* findChannel(void* handle) const;
    MockEncoder* findEncoder(void* handle);
    const MockEncoder* findEncoder(void* handle) const;
};

//=============================================================================
// Singleton Instance
//=============================================================================

Rmt5PeripheralMock& Rmt5PeripheralMock::instance() {
    return Singleton<Rmt5PeripheralMockImpl>::instance();
}

//=============================================================================
// Constructor / Destructor
//=============================================================================

Rmt5PeripheralMockImpl::Rmt5PeripheralMockImpl()
    : mChannels(),
      mEncoders(),
      mNextChannelId(1),
      mNextEncoderId(1),
      mShouldFailTransmit(false),
      mHistory(),
      mTransmissionCount(0) {
}

Rmt5PeripheralMockImpl::~Rmt5PeripheralMockImpl() {
    // Delete all allocated channels and encoders
    for (auto pair : mChannels) {
        delete pair.second;
    }
    for (auto pair : mEncoders) {
        delete pair.second;
    }
}

//=============================================================================
// Helper Methods
//=============================================================================

MockChannel* Rmt5PeripheralMockImpl::findChannel(void* handle) {
    if (handle == nullptr) {
        return nullptr;
    }
    intptr_t handle_as_int = fl::bit_cast<intptr_t>(handle);
    int id = static_cast<int>(handle_as_int);
    auto it = mChannels.find(id);
    if (it == mChannels.end()) {
        return nullptr;
    }
    return it->second;  // Return the stored pointer directly
}

const MockChannel* Rmt5PeripheralMockImpl::findChannel(void* handle) const {
    if (handle == nullptr) {
        return nullptr;
    }
    int id = fl::bit_cast<intptr_t>(handle);
    auto it = mChannels.find(id);
    if (it == mChannels.end()) {
        return nullptr;
    }
    return it->second;  // Return the stored pointer directly
}

MockEncoder* Rmt5PeripheralMockImpl::findEncoder(void* handle) {
    if (handle == nullptr) {
        return nullptr;
    }
    int id = fl::bit_cast<intptr_t>(handle);
    auto it = mEncoders.find(id);
    if (it == mEncoders.end()) {
        return nullptr;
    }
    return it->second;  // Return the stored pointer directly
}

const MockEncoder* Rmt5PeripheralMockImpl::findEncoder(void* handle) const {
    if (handle == nullptr) {
        return nullptr;
    }
    int id = fl::bit_cast<intptr_t>(handle);
    auto it = mEncoders.find(id);
    if (it == mEncoders.end()) {
        return nullptr;
    }
    return it->second;  // Return the stored pointer directly
}

//=============================================================================
// Channel Lifecycle Methods
//=============================================================================

bool Rmt5PeripheralMockImpl::createTxChannel(const Rmt5ChannelConfig& config,
                                               void** out_handle) {
    if (out_handle == nullptr) {
        FL_WARN("Rmt5PeripheralMock: out_handle is nullptr");
        return false;
    }

    // Validate config
    if (config.gpio_num < 0) {
        FL_WARN("Rmt5PeripheralMock: Invalid GPIO pin: " << config.gpio_num);
        return false;
    }

    // Create mock channel
    int channel_id = mNextChannelId++;
    MockChannel* channel = new MockChannel();
    channel->id = channel_id;
    channel->config = config;
    channel->enabled = false;
    channel->callback = nullptr;
    channel->user_ctx = nullptr;

    mChannels[channel_id] = channel;

    // Return opaque handle
    *out_handle = fl::bit_cast<void*>(static_cast<intptr_t>(channel_id));

    FL_DBG("RMT5_MOCK: Created TX channel " << channel_id
           << " on GPIO " << config.gpio_num
           << " (DMA: " << config.with_dma << ") handle=" << *out_handle);

    return true;
}

bool Rmt5PeripheralMockImpl::deleteChannel(void* channel_handle) {
    MockChannel* channel = findChannel(channel_handle);
    if (channel == nullptr) {
        FL_WARN("Rmt5PeripheralMock: Invalid channel handle");
        return false;
    }

    int id = channel->id;
    delete channel;  // Free the allocated memory
    mChannels.erase(id);

    FL_DBG("RMT5_MOCK: Deleted channel " << id);
    return true;
}

bool Rmt5PeripheralMockImpl::enableChannel(void* channel_handle) {
    MockChannel* channel = findChannel(channel_handle);
    if (channel == nullptr) {
        FL_WARN("Rmt5PeripheralMock: Invalid channel handle");
        return false;
    }

    channel->enabled = true;
    FL_DBG("RMT5_MOCK: Enabled channel " << channel->id);
    return true;
}

bool Rmt5PeripheralMockImpl::disableChannel(void* channel_handle) {
    MockChannel* channel = findChannel(channel_handle);
    if (channel == nullptr) {
        FL_WARN("Rmt5PeripheralMock: Invalid channel handle");
        return false;
    }

    channel->enabled = false;
    FL_DBG("RMT5_MOCK: Disabled channel " << channel->id);
    return true;
}

//=============================================================================
// Transmission Methods
//=============================================================================

bool Rmt5PeripheralMockImpl::transmit(void* channel_handle, void* encoder_handle,
                                       const u8* buffer, size_t buffer_size) {
    // Error injection
    if (mShouldFailTransmit) {
        FL_WARN("Rmt5PeripheralMock: Transmit failure injected");
        return false;
    }

    MockChannel* channel = findChannel(channel_handle);
    if (channel == nullptr) {
        FL_WARN("Rmt5PeripheralMock: Invalid channel handle");
        return false;
    }

    MockEncoder* encoder = findEncoder(encoder_handle);
    if (encoder == nullptr) {
        FL_WARN("Rmt5PeripheralMock: Invalid encoder handle");
        return false;
    }

    if (buffer == nullptr || buffer_size == 0) {
        FL_WARN("Rmt5PeripheralMock: Invalid buffer");
        return false;
    }

    if (!channel->enabled) {
        FL_WARN("Rmt5PeripheralMock: Channel not enabled");
        return false;
    }

    // Capture transmission data
    TransmissionRecord record;
    record.buffer_copy.resize(buffer_size);
    fl::memcpy(record.buffer_copy.data(), buffer, buffer_size);
    record.buffer_size = buffer_size;
    record.gpio_pin = channel->config.gpio_num;
    record.timing = encoder->timing;
    record.resolution_hz = encoder->resolution_hz;
    record.used_dma = channel->config.with_dma;

    #ifdef ARDUINO
    record.timestamp_us = fl::micros();
    #else
    record.timestamp_us = stub::fl::micros();
    #endif

    mHistory.push_back(record);
    mTransmissionCount++;

    FL_DBG("RMT5_MOCK: Transmitted " << buffer_size << " bytes on channel "
           << channel->id << " (pin " << channel->config.gpio_num << ")");

    return true;
}

bool Rmt5PeripheralMockImpl::waitAllDone(void* channel_handle, u32 timeout_ms) {
    (void)timeout_ms;  // Mock always returns immediately

    MockChannel* channel = findChannel(channel_handle);
    if (channel == nullptr) {
        FL_WARN("Rmt5PeripheralMock: Invalid channel handle");
        return false;
    }

    // Mock implementation: Always return true immediately
    // (transmission is instant in mock)
    FL_DBG("RMT5_MOCK: Wait all done for channel " << channel->id);
    return true;
}

//=============================================================================
// ISR Callback Registration
//=============================================================================

bool Rmt5PeripheralMockImpl::registerTxCallback(void* channel_handle,
                                                 Rmt5TxDoneCallback callback,
                                                 void* user_ctx) {
    MockChannel* channel = findChannel(channel_handle);
    if (channel == nullptr) {
        FL_WARN("Rmt5PeripheralMock: Invalid channel handle");
        return false;
    }

    channel->callback = callback;
    channel->user_ctx = user_ctx;

    FL_DBG("RMT5_MOCK: Registered TX callback for channel " << channel->id);
    return true;
}

//=============================================================================
// Platform Configuration
//=============================================================================

void Rmt5PeripheralMockImpl::configureLogging() {
    // Mock implementation: No-op (host platforms don't have ESP-IDF logging)
    FL_DBG("RMT5_MOCK: Logging configuration (no-op on mock platform)");
}

bool Rmt5PeripheralMockImpl::syncCache(void* buffer, size_t size) {
    // Mock implementation: No-op (host platforms don't have DMA or cache sync)
    (void)buffer;
    (void)size;
    FL_DBG("RMT5_MOCK: Cache sync (no-op on mock platform)");
    return true;  // Always succeeds
}

//=============================================================================
// Encoder Management
//=============================================================================

void* Rmt5PeripheralMockImpl::createEncoder(const ChipsetTiming& timing,
                                              u32 resolution_hz) {
    // Create mock encoder
    int encoder_id = mNextEncoderId++;
    MockEncoder* encoder = new MockEncoder();
    encoder->id = encoder_id;
    encoder->timing = timing;
    encoder->resolution_hz = resolution_hz;

    mEncoders[encoder_id] = encoder;

    // Return opaque handle
    void* handle = fl::bit_cast<void*>(static_cast<intptr_t>(encoder_id));

    FL_DBG("RMT5_MOCK: Created encoder " << encoder_id
           << " (resolution: " << resolution_hz << " Hz)");

    return handle;
}

void Rmt5PeripheralMockImpl::deleteEncoder(void* encoder_handle) {
    MockEncoder* encoder = findEncoder(encoder_handle);
    if (encoder == nullptr) {
        return;  // Safe no-op
    }

    int id = encoder->id;
    delete encoder;  // Free the allocated memory
    mEncoders.erase(id);

    FL_DBG("RMT5_MOCK: Deleted encoder " << id);
}

bool Rmt5PeripheralMockImpl::resetEncoder(void* encoder_handle) {
    MockEncoder* encoder = findEncoder(encoder_handle);
    if (encoder == nullptr) {
        FL_WARN("Rmt5PeripheralMock: Invalid encoder handle");
        return false;
    }

    // Mock implementation: Encoder reset is a no-op (mock encoder has no state machine)
    FL_DBG("RMT5_MOCK: Reset encoder " << encoder->id);
    return true;  // Always succeeds
}

//=============================================================================
// DMA Memory Management
//=============================================================================

u8* Rmt5PeripheralMockImpl::allocateDmaBuffer(size_t size) {
    if (size == 0) {
        FL_WARN("Rmt5PeripheralMock: Cannot allocate zero-size buffer");
        return nullptr;
    }

    // Round up to 64-byte alignment
    const size_t alignment = 64;
    size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);

    // Allocate aligned memory
    #ifdef FL_IS_WIN
    u8* buffer = static_cast<u8*>(_aligned_malloc(aligned_size, alignment));
    #else
    u8* buffer = static_cast<u8*>(aligned_alloc(alignment, aligned_size));
    #endif

    if (buffer == nullptr) {
        FL_WARN("Rmt5PeripheralMock: Failed to allocate DMA buffer ("
                << aligned_size << " bytes)");
        return nullptr;
    }

    FL_DBG("RMT5_MOCK: Allocated DMA buffer (" << aligned_size << " bytes)");
    return buffer;
}

void Rmt5PeripheralMockImpl::freeDmaBuffer(u8* buffer) {
    if (buffer == nullptr) {
        return;  // Safe no-op
    }

    #ifdef FL_IS_WIN
    _aligned_free(buffer);
    #else
    free(buffer);
    #endif

    FL_DBG("RMT5_MOCK: Freed DMA buffer");
}

//=============================================================================
// Mock-Specific API
//=============================================================================

void Rmt5PeripheralMockImpl::simulateTransmitDone(void* channel_handle) {
    MockChannel* channel = findChannel(channel_handle);
    if (channel == nullptr) {
        FL_WARN("Rmt5PeripheralMock: Invalid channel handle");
        return;
    }

    if (channel->callback == nullptr) {
        FL_DBG("RMT5_MOCK: No callback registered for channel " << channel->id);
        return;
    }

    FL_DBG("RMT5_MOCK: Triggering TX callback for channel " << channel->id);
    // Pass nullptr for event_data (matches ESP-IDF behavior for simple transmissions)
    channel->callback(channel_handle, nullptr, channel->user_ctx);
}

void Rmt5PeripheralMockImpl::setTransmitFailure(bool should_fail) {
    mShouldFailTransmit = should_fail;
    FL_DBG("RMT5_MOCK: Transmit failure " << (should_fail ? "enabled" : "disabled"));
}

const fl::vector<Rmt5PeripheralMock::TransmissionRecord>&
Rmt5PeripheralMockImpl::getTransmissionHistory() const {
    return mHistory;
}

void Rmt5PeripheralMockImpl::clearTransmissionHistory() {
    mHistory.clear();
    FL_DBG("RMT5_MOCK: Cleared transmission history");
}

fl::span<const u8> Rmt5PeripheralMockImpl::getLastTransmissionData() const {
    if (mHistory.empty()) {
        return fl::span<const u8>();
    }
    const auto& last = mHistory.back();
    return fl::span<const u8>(last.buffer_copy.data(), last.buffer_copy.size());
}

size_t Rmt5PeripheralMockImpl::getChannelCount() const {
    return mChannels.size();
}

size_t Rmt5PeripheralMockImpl::getEncoderCount() const {
    return mEncoders.size();
}

size_t Rmt5PeripheralMockImpl::getTransmissionCount() const {
    return mTransmissionCount;
}

bool Rmt5PeripheralMockImpl::isChannelEnabled(void* channel_handle) const {
    const MockChannel* channel = findChannel(channel_handle);
    if (channel == nullptr) {
        return false;
    }
    return channel->enabled;
}

void Rmt5PeripheralMockImpl::reset() {
    // Delete all allocated channels
    for (auto pair : mChannels) {
        delete pair.second;
    }
    mChannels.clear();

    // Delete all allocated encoders
    for (auto pair : mEncoders) {
        delete pair.second;
    }
    mEncoders.clear();

    mHistory.clear();
    mNextChannelId = 1;
    mNextEncoderId = 1;
    mShouldFailTransmit = false;
    mTransmissionCount = 0;

    FL_DBG("RMT5_MOCK: Reset to initial state");
}

} // namespace detail
} // namespace fl

#endif // FASTLED_STUB_IMPL || host platform
