// IWYU pragma: private

/// @file rmt5_peripheral_mock.cpp.hpp
/// @brief Mock RMT5 peripheral implementation for unit testing

// This mock is only for host testing
// Compile for stub platform testing OR non-Arduino host platforms
#include "platforms/is_platform.h"
#if defined(FASTLED_STUB_IMPL) || (!defined(ARDUINO) && (defined(FL_IS_LINUX) || defined(FL_IS_APPLE) || defined(FL_IS_WIN)))

#include "platforms/shared/mock/esp/32/drivers/rmt5_peripheral_mock.h"
#include "fl/log/log.h"
#include "fl/stl/allocator.h"
#include "fl/stl/bit_cast.h"
#include "fl/stl/cstring.h"
#include "fl/stl/singleton.h"
#include "fl/stl/flat_map.h"

#include "fl/stl/cstdlib.h"

#ifdef ARDUINO
#include "fl/system/arduino.h"
#else
#include "platforms/stub/time_stub.h"  // For fl::micros() on host tests
#include "fl/stl/noexcept.h"
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
 FL_NO_EXCEPT : id(0), config(), enabled(false), callback(nullptr), user_ctx(nullptr) {}
};

struct MockEncoder {
    int id;
    ChipsetTiming timing;
    u32 resolution_hz;

    MockEncoder()
 FL_NO_EXCEPT : id(0), timing(), resolution_hz(0) {}
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

    Rmt5PeripheralMockImpl() FL_NO_EXCEPT;
    ~Rmt5PeripheralMockImpl() override;

    //=========================================================================
    // IRMT5Peripheral Interface Implementation
    //=========================================================================

    bool createTxChannel(const Rmt5ChannelConfig& config,
                         void** out_handle) FL_NO_EXCEPT override;
    bool deleteChannel(void* channel_handle) FL_NO_EXCEPT override;
    bool enableChannel(void* channel_handle) FL_NO_EXCEPT override;
    bool disableChannel(void* channel_handle) FL_NO_EXCEPT override;
    bool transmit(void* channel_handle, void* encoder_handle,
                  fl::span<const u8> buffer) FL_NO_EXCEPT override;
    bool waitAllDone(void* channel_handle, u32 timeout_ms) FL_NO_EXCEPT override;
    void* createEncoder(const ChipsetTiming& timing,
                        u32 resolution_hz) FL_NO_EXCEPT override;
    void deleteEncoder(void* encoder_handle) FL_NO_EXCEPT override;
    bool resetEncoder(void* encoder_handle) FL_NO_EXCEPT override;
    bool registerTxCallback(void* channel_handle,
                            Rmt5TxDoneCallback callback,
                            void* user_ctx) FL_NO_EXCEPT override;
    void configureLogging() FL_NO_EXCEPT override;
    bool syncCache(fl::span<u8> buffer) FL_NO_EXCEPT override;
    bool canTransmitDirectly(const void* buffer) const FL_NO_EXCEPT override {
        (void)buffer;
        return mDirectTransmission;
    }
    u8* allocateDmaBuffer(size_t size) FL_NO_EXCEPT override;
    void freeDmaBuffer(u8* buffer) FL_NO_EXCEPT override;

    //=========================================================================
    // Mock-Specific API
    //=========================================================================

    void simulateTransmitDone(void* channel_handle) FL_NO_EXCEPT override;
    void setTransmitFailure(bool should_fail) FL_NO_EXCEPT override;
    void setEnableFailure(bool should_fail) FL_NO_EXCEPT override {
        mShouldFailEnable = should_fail;
    }
    void setResetFailure(bool should_fail) FL_NO_EXCEPT override {
        mShouldFailReset = should_fail;
    }
    void setEncoderFailure(bool should_fail) FL_NO_EXCEPT override {
        mShouldFailEncoder = should_fail;
    }
    void setCallbackFailure(bool should_fail) FL_NO_EXCEPT override {
        mShouldFailCallback = should_fail;
    }
    void setDirectTransmission(bool supported) FL_NO_EXCEPT override {
        mDirectTransmission = supported;
    }
    void setMaxChannels(size_t max_channels) FL_NO_EXCEPT override;
    const fl::vector<TransmissionRecord>& getTransmissionHistory() const FL_NO_EXCEPT override;
    void clearTransmissionHistory() FL_NO_EXCEPT override;
    fl::span<const u8> getLastTransmissionData() const FL_NO_EXCEPT override;
    size_t getChannelCount() const FL_NO_EXCEPT override;
    size_t getEncoderCount() const FL_NO_EXCEPT override;
    size_t getTransmissionCount() const FL_NO_EXCEPT override;
    bool isChannelEnabled(void* channel_handle) const FL_NO_EXCEPT override;
    void reset() FL_NO_EXCEPT override;

private:
    //=========================================================================
    // Internal State
    //=========================================================================

    // Channel and encoder management (store pointers for stable references)
    fl::flat_map<int, MockChannel*> mChannels;
    fl::flat_map<int, MockEncoder*> mEncoders;
    int mNextChannelId;
    int mNextEncoderId;

    // Simulation settings
    bool mShouldFailTransmit;
    bool mShouldFailEnable = false;
    bool mShouldFailReset = false;
    bool mShouldFailEncoder = false;
    bool mShouldFailCallback = false;
    bool mDirectTransmission = false;
    size_t mMaxChannels;  // 0 = unlimited

    // Waveform capture
    fl::vector<TransmissionRecord> mHistory;
    size_t mTransmissionCount;

    // Helper methods
    MockChannel* findChannel(void* handle) FL_NO_EXCEPT;
    const MockChannel* findChannel(void* handle) const FL_NO_EXCEPT;
    MockEncoder* findEncoder(void* handle) FL_NO_EXCEPT;
    const MockEncoder* findEncoder(void* handle) const FL_NO_EXCEPT;
};

//=============================================================================
// Singleton Instance
//=============================================================================

Rmt5PeripheralMock& Rmt5PeripheralMock::instance() FL_NO_EXCEPT {
    return Singleton<Rmt5PeripheralMockImpl>::instance();
}

//=============================================================================
// Constructor / Destructor
//=============================================================================

Rmt5PeripheralMockImpl::Rmt5PeripheralMockImpl()
 FL_NO_EXCEPT : mChannels(),
      mEncoders(),
      mNextChannelId(1),
      mNextEncoderId(1),
      mShouldFailTransmit(false),
      mMaxChannels(0),
      mHistory(),
      mTransmissionCount(0) {
}

Rmt5PeripheralMockImpl::~Rmt5PeripheralMockImpl() {
    // Delete all allocated channels and encoders
    for (auto pair : mChannels) {
        delete pair.second;  // ok bare allocation
    }
    for (auto pair : mEncoders) {
        delete pair.second;  // ok bare allocation
    }
}

//=============================================================================
// Helper Methods
//=============================================================================

MockChannel* Rmt5PeripheralMockImpl::findChannel(void* handle) FL_NO_EXCEPT {
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

const MockChannel* Rmt5PeripheralMockImpl::findChannel(void* handle) const FL_NO_EXCEPT {
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

MockEncoder* Rmt5PeripheralMockImpl::findEncoder(void* handle) FL_NO_EXCEPT {
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

const MockEncoder* Rmt5PeripheralMockImpl::findEncoder(void* handle) const FL_NO_EXCEPT {
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
                                               void** out_handle) FL_NO_EXCEPT {
    if (out_handle == nullptr) {
        FL_WARN("Rmt5PeripheralMock: out_handle is nullptr");
        return false;
    }

    // Validate config
    if (config.gpio_num < 0) {
        FL_WARN("Rmt5PeripheralMock: Invalid GPIO pin: " << config.gpio_num);
        return false;
    }
    if (mMaxChannels != 0 && mChannels.size() >= mMaxChannels) {
        FL_DBG("RMT5_MOCK: TX channel limit reached (" << mMaxChannels << ")");
        return false;
    }

    // Create mock channel
    int channel_id = mNextChannelId++;
    MockChannel* channel = new MockChannel();  // ok bare allocation
    channel->id = channel_id;
    channel->config = config;
    channel->enabled = false;
    channel->callback = nullptr;
    channel->user_ctx = nullptr;

    mChannels[channel_id] = channel;

    // Return opaque handle
    *out_handle = fl::bit_cast<void*>(static_cast<intptr_t>(channel_id));

    FL_DBG("RMT5_MOCK: Created TX channel " << channel_id << " on GPIO " << config.gpio_num << " (DMA: " << config.with_dma << ") handle=" << (*out_handle));

    return true;
}

bool Rmt5PeripheralMockImpl::deleteChannel(void* channel_handle) FL_NO_EXCEPT {
    MockChannel* channel = findChannel(channel_handle);
    if (channel == nullptr) {
        FL_WARN("Rmt5PeripheralMock: Invalid channel handle");
        return false;
    }

    int id = channel->id;
    delete channel;  // Free the allocated memory  // ok bare allocation
    mChannels.erase(id);

    FL_DBG("RMT5_MOCK: Deleted channel " << id);
    return true;
}

bool Rmt5PeripheralMockImpl::enableChannel(void* channel_handle) FL_NO_EXCEPT {
    if (mShouldFailEnable) {
        return false;
    }

    MockChannel* channel = findChannel(channel_handle);
    if (channel == nullptr) {
        FL_WARN("Rmt5PeripheralMock: Invalid channel handle");
        return false;
    }

    channel->enabled = true;
    FL_DBG("RMT5_MOCK: Enabled channel " << channel->id);
    return true;
}

bool Rmt5PeripheralMockImpl::disableChannel(void* channel_handle) FL_NO_EXCEPT {
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
                                       fl::span<const u8> buffer) FL_NO_EXCEPT {
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

    if (buffer.data() == nullptr || buffer.size() == 0) {
        FL_WARN("Rmt5PeripheralMock: Invalid buffer");
        return false;
    }

    if (!channel->enabled) {
        FL_WARN("Rmt5PeripheralMock: Channel not enabled");
        return false;
    }

    // Capture transmission data
    TransmissionRecord record;
    record.buffer_copy.assign(buffer.begin(), buffer.end());
    record.channel_address = fl::ptr_to_int(channel_handle);
    record.buffer_address = fl::ptr_to_int(buffer.data());
    record.buffer_size = buffer.size();
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

    FL_DBG("RMT5_MOCK: Transmitted " << buffer.size() << " bytes on channel " << channel->id << " (pin " << channel->config.gpio_num << ")");

    return true;
}

bool Rmt5PeripheralMockImpl::waitAllDone(void* channel_handle, u32 timeout_ms) FL_NO_EXCEPT {
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
                                                 void* user_ctx) FL_NO_EXCEPT {
    if (mShouldFailCallback) {
        return false;
    }

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

void Rmt5PeripheralMockImpl::configureLogging() FL_NO_EXCEPT {
    // Mock implementation: No-op (host platforms don't have ESP-IDF logging)
    FL_DBG("RMT5_MOCK: Logging configuration (no-op on mock platform)");
}

bool Rmt5PeripheralMockImpl::syncCache(fl::span<u8> buffer) FL_NO_EXCEPT {
    // Mock implementation: No-op (host platforms don't have DMA or cache sync)
    (void)buffer;
    FL_DBG("RMT5_MOCK: Cache sync (no-op on mock platform)");
    return true;  // Always succeeds
}

//=============================================================================
// Encoder Management
//=============================================================================

void* Rmt5PeripheralMockImpl::createEncoder(const ChipsetTiming& timing,
                                              u32 resolution_hz) FL_NO_EXCEPT {
    if (mShouldFailEncoder) {
        return nullptr;
    }

    // Create mock encoder
    int encoder_id = mNextEncoderId++;
    MockEncoder* encoder = new MockEncoder();  // ok bare allocation
    encoder->id = encoder_id;
    encoder->timing = timing;
    encoder->resolution_hz = resolution_hz;

    mEncoders[encoder_id] = encoder;

    // Return opaque handle
    void* handle = fl::bit_cast<void*>(static_cast<intptr_t>(encoder_id));

    FL_DBG("RMT5_MOCK: Created encoder " << encoder_id << " (resolution: " << resolution_hz << " Hz)");

    return handle;
}

void Rmt5PeripheralMockImpl::deleteEncoder(void* encoder_handle) FL_NO_EXCEPT {
    MockEncoder* encoder = findEncoder(encoder_handle);
    if (encoder == nullptr) {
        return;  // Safe no-op
    }

    int id = encoder->id;
    delete encoder;  // Free the allocated memory  // ok bare allocation
    mEncoders.erase(id);

    FL_DBG("RMT5_MOCK: Deleted encoder " << id);
}

bool Rmt5PeripheralMockImpl::resetEncoder(void* encoder_handle) FL_NO_EXCEPT {
    if (mShouldFailReset) {
        return false;
    }

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

u8* Rmt5PeripheralMockImpl::allocateDmaBuffer(size_t size) FL_NO_EXCEPT {
    if (size == 0) {
        FL_WARN("Rmt5PeripheralMock: Cannot allocate zero-size buffer");
        return nullptr;
    }

    // Round up to 64-byte alignment
    const size_t alignment = 64;
    size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);

    // Allocate aligned memory
    u8* buffer = static_cast<u8*>(fl::aligned_alloc(alignment, aligned_size));

    if (buffer == nullptr) {
        FL_WARN("Rmt5PeripheralMock: Failed to allocate DMA buffer (" << aligned_size << " bytes)");
        return nullptr;
    }

    FL_DBG("RMT5_MOCK: Allocated DMA buffer (" << aligned_size << " bytes)");
    return buffer;
}

void Rmt5PeripheralMockImpl::freeDmaBuffer(u8* buffer) FL_NO_EXCEPT {
    if (buffer == nullptr) {
        return;  // Safe no-op
    }

    fl::aligned_free(buffer);

    FL_DBG("RMT5_MOCK: Freed DMA buffer");
}

//=============================================================================
// Mock-Specific API
//=============================================================================

void Rmt5PeripheralMockImpl::simulateTransmitDone(void* channel_handle) FL_NO_EXCEPT {
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

void Rmt5PeripheralMockImpl::setTransmitFailure(bool should_fail) FL_NO_EXCEPT {
    mShouldFailTransmit = should_fail;
    FL_DBG("RMT5_MOCK: Transmit failure " << ((should_fail ? "enabled" : "disabled")));
}

void Rmt5PeripheralMockImpl::setMaxChannels(size_t max_channels) FL_NO_EXCEPT {
    mMaxChannels = max_channels;
}

const fl::vector<Rmt5PeripheralMock::TransmissionRecord>&
Rmt5PeripheralMockImpl::getTransmissionHistory() const FL_NO_EXCEPT {
    return mHistory;
}

void Rmt5PeripheralMockImpl::clearTransmissionHistory() FL_NO_EXCEPT {
    mHistory.clear();
    FL_DBG("RMT5_MOCK: Cleared transmission history");
}

fl::span<const u8> Rmt5PeripheralMockImpl::getLastTransmissionData() const FL_NO_EXCEPT {
    if (mHistory.empty()) {
        return fl::span<const u8>();
    }
    const auto& last = mHistory.back();
    return last.buffer_copy;
}

size_t Rmt5PeripheralMockImpl::getChannelCount() const FL_NO_EXCEPT {
    return mChannels.size();
}

size_t Rmt5PeripheralMockImpl::getEncoderCount() const FL_NO_EXCEPT {
    return mEncoders.size();
}

size_t Rmt5PeripheralMockImpl::getTransmissionCount() const FL_NO_EXCEPT {
    return mTransmissionCount;
}

bool Rmt5PeripheralMockImpl::isChannelEnabled(void* channel_handle) const FL_NO_EXCEPT {
    const MockChannel* channel = findChannel(channel_handle);
    if (channel == nullptr) {
        return false;
    }
    return channel->enabled;
}

void Rmt5PeripheralMockImpl::reset() FL_NO_EXCEPT {
    // Delete all allocated channels
    for (auto pair : mChannels) {
        delete pair.second;  // ok bare allocation
    }
    mChannels.clear();

    // Delete all allocated encoders
    for (auto pair : mEncoders) {
        delete pair.second;  // ok bare allocation
    }
    mEncoders.clear();

    mHistory.clear();
    mNextChannelId = 1;
    mNextEncoderId = 1;
    mShouldFailTransmit = false;
    mShouldFailEnable = false;
    mShouldFailReset = false;
    mShouldFailEncoder = false;
    mShouldFailCallback = false;
    mDirectTransmission = false;
    mMaxChannels = 0;
    mTransmissionCount = 0;

    FL_DBG("RMT5_MOCK: Reset to initial state");
}

} // namespace detail
} // namespace fl

#endif // FASTLED_STUB_IMPL || host platform
