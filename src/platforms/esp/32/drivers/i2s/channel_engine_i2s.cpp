/// @file channel_engine_i2s.cpp
/// @brief I2S LCD_CAM channel engine implementation for ESP32-S3

// Compile for:
// 1. ESP32-S3 with LCD_CAM support (real hardware)
// 2. Stub/host platform testing (FASTLED_STUB_IMPL or host OS)
#if defined(ESP32)
#include "sdkconfig.h"
#endif

#if defined(CONFIG_IDF_TARGET_ESP32S3) || \
    defined(FASTLED_STUB_IMPL) || \
    (!defined(ARDUINO) && (defined(__linux__) || defined(__APPLE__) || defined(_WIN32)))

#include "channel_engine_i2s.h"
#include "fl/warn.h"
#include "fl/stl/cstring.h"

// Include ESP implementation only on real hardware
#if defined(CONFIG_IDF_TARGET_ESP32S3)
#include "i2s_lcd_cam_peripheral_esp.h"
#endif

namespace fl {

//=============================================================================
// Transpose Constants (from Yves driver)
//=============================================================================

// Transpose bit manipulation constants
#define AA (0x00AA00AAL)
#define CC (0x0000CCCCL)
#define FF (0xF0F0F0F0L)
#define FF2 (0x0F0F0F0FL)

//=============================================================================
// Constructor / Destructor
//=============================================================================

ChannelEngineI2S::ChannelEngineI2S(fl::shared_ptr<detail::II2sLcdCamPeripheral> peripheral)
    : mPeripheral(fl::move(peripheral)),
      mInitialized(false),
      mConfig(),
      mNumLeds(0),
      mNumComponents(3),
      mStrips{},
      mScratchBuffer(),
      mBuffers{nullptr, nullptr},
      mBufferSize(0),
      mFrontBuffer(0),
      mEnqueuedChannels(),
      mTransmittingChannels(),
      mChipsetGroups(),
      mCurrentGroupIndex(0),
      mBusy(false),
      mFrameCounter(0) {
    for (int i = 0; i < 16; i++) {
        mStrips[i] = nullptr;
        mConfig.data_gpios[i] = -1;
    }
}

ChannelEngineI2S::~ChannelEngineI2S() {
    // Wait for pending transmission
    while (mBusy) {
        poll();
    }

    // Free buffers
    if (mPeripheral) {
        for (int i = 0; i < 2; i++) {
            if (mBuffers[i] != nullptr) {
                mPeripheral->freeBuffer(mBuffers[i]);
                mBuffers[i] = nullptr;
            }
        }
    }
}

//=============================================================================
// IChannelEngine Interface
//=============================================================================

void ChannelEngineI2S::enqueue(ChannelDataPtr channelData) {
    mEnqueuedChannels.push_back(fl::move(channelData));
}

void ChannelEngineI2S::show() {
    if (mEnqueuedChannels.empty()) {
        return;
    }

    // Wait for previous transmission to complete
    while (poll() != EngineState::READY) {
        // Busy wait - poll() handles state transitions
    }

    // Group channels by timing configuration
    mChipsetGroups.clear();
    for (auto& channel : mEnqueuedChannels) {
        bool found = false;
        for (auto& group : mChipsetGroups) {
            if (group.mTiming == channel->getTiming()) {
                group.mChannels.push_back(channel);
                found = true;
                break;
            }
        }
        if (!found) {
            ChipsetGroup newGroup(channel->getTiming());
            newGroup.mChannels.push_back(channel);
            mChipsetGroups.push_back(fl::move(newGroup));
        }
    }

    // Move enqueued to transmitting
    mTransmittingChannels = fl::move(mEnqueuedChannels);
    mEnqueuedChannels.clear();

    // Start transmission of first group
    mCurrentGroupIndex = 0;
    if (!mChipsetGroups.empty()) {
        if (!beginTransmission(mChipsetGroups[0].mChannels)) {
            // Transmission failed - clean up
            for (auto& channel : mTransmittingChannels) {
                channel->setInUse(false);
            }
            mTransmittingChannels.clear();
            mChipsetGroups.clear();
        }
    }
}

IChannelEngine::EngineState ChannelEngineI2S::poll() {
    if (mTransmittingChannels.empty()) {
        return EngineState::READY;
    }

    // Check if current transmission is complete
    if (mPeripheral && !mPeripheral->isBusy()) {
        mBusy = false;

        // Move to next chipset group if available
        mCurrentGroupIndex++;
        if (mCurrentGroupIndex < mChipsetGroups.size()) {
            if (!beginTransmission(mChipsetGroups[mCurrentGroupIndex].mChannels)) {
                // Transmission of next group failed - clean up all remaining
                for (auto& channel : mTransmittingChannels) {
                    channel->setInUse(false);
                }
                mTransmittingChannels.clear();
                mChipsetGroups.clear();
                return EngineState::READY;
            }
            return EngineState::BUSY;
        }

        // All groups complete - clean up
        for (auto& channel : mTransmittingChannels) {
            channel->setInUse(false);
        }
        mTransmittingChannels.clear();
        mChipsetGroups.clear();
        return EngineState::READY;
    }

    return mBusy ? EngineState::DRAINING : EngineState::READY;
}

//=============================================================================
// Transpose Implementation (from Yves driver)
//=============================================================================

void ChannelEngineI2S::transpose16x1(const uint8_t* A, uint16_t* B) {
    uint32_t x, y, x1, y1, t;

    // Load the array - handle up to 16 strips
    y = *(reinterpret_cast<const uint32_t*>(A)); // ok reinterpret cast
    x = *(reinterpret_cast<const uint32_t*>(A + 4)); // ok reinterpret cast
    y1 = *(reinterpret_cast<const uint32_t*>(A + 8)); // ok reinterpret cast
    x1 = *(reinterpret_cast<const uint32_t*>(A + 12)); // ok reinterpret cast

    // Pre-transform x
    t = (x ^ (x >> 7)) & AA;
    x = x ^ t ^ (t << 7);
    t = (x ^ (x >> 14)) & CC;
    x = x ^ t ^ (t << 14);

    t = (x1 ^ (x1 >> 7)) & AA;
    x1 = x1 ^ t ^ (t << 7);
    t = (x1 ^ (x1 >> 14)) & CC;
    x1 = x1 ^ t ^ (t << 14);

    // Pre-transform y
    t = (y ^ (y >> 7)) & AA;
    y = y ^ t ^ (t << 7);
    t = (y ^ (y >> 14)) & CC;
    y = y ^ t ^ (t << 14);

    t = (y1 ^ (y1 >> 7)) & AA;
    y1 = y1 ^ t ^ (t << 7);
    t = (y1 ^ (y1 >> 14)) & CC;
    y1 = y1 ^ t ^ (t << 14);

    // Final transform
    t = (x & FF) | ((y >> 4) & FF2);
    y = ((x << 4) & FF) | (y & FF2);
    x = t;

    t = (x1 & FF) | ((y1 >> 4) & FF2);
    y1 = ((x1 << 4) & FF) | (y1 & FF2);
    x1 = t;

    // Store transposed result - 8 output words (for 8 bits per color component)
    B[0] = static_cast<uint16_t>(((x & 0xff000000) >> 8 | ((x1 & 0xff000000))) >> 16);
    B[3] = static_cast<uint16_t>(((x & 0xff0000) >> 16 | ((x1 & 0xff0000) >> 8)));
    B[6] = static_cast<uint16_t>(((x & 0xff00) | ((x1 & 0xff00) << 8)) >> 8);
    B[9] = static_cast<uint16_t>((x & 0xff) | ((x1 & 0xff) << 8));
    B[12] = static_cast<uint16_t>(((y & 0xff000000) >> 8 | ((y1 & 0xff000000))) >> 16);
    B[15] = static_cast<uint16_t>(((y & 0xff0000) | ((y1 & 0xff0000) << 8)) >> 16);
    B[18] = static_cast<uint16_t>(((y & 0xff00) | ((y1 & 0xff00) << 8)) >> 8);
    B[21] = static_cast<uint16_t>((y & 0xff) | ((y1 & 0xff) << 8));
}

//=============================================================================
// Transmission Implementation
//=============================================================================

bool ChannelEngineI2S::beginTransmission(fl::span<const ChannelDataPtr> channelData) {
    if (channelData.empty() || !mPeripheral) {
        return false;
    }

    // Find maximum channel size
    size_t maxChannelSize = 0;
    for (const auto& channel : channelData) {
        size_t sz = channel->getSize();
        if (sz > maxChannelSize) {
            maxChannelSize = sz;
        }
    }

    if (maxChannelSize == 0) {
        return false;
    }

    // Calculate number of LEDs (RGB = 3 bytes per LED)
    int numLeds = static_cast<int>(maxChannelSize / 3);
    int numLanes = static_cast<int>(channelData.size());

    // Initialize or reconfigure if needed
    bool needsInit = !mInitialized ||
                     mNumLeds != numLeds ||
                     mConfig.num_lanes != numLanes;

    if (needsInit) {
        // Free old buffers
        for (int i = 0; i < 2; i++) {
            if (mBuffers[i] != nullptr) {
                mPeripheral->freeBuffer(mBuffers[i]);
                mBuffers[i] = nullptr;
            }
        }

        // Configure
        mNumLeds = numLeds;
        mNumComponents = 3;  // RGB
        mConfig.num_lanes = numLanes;
        mConfig.use_psram = true;

        // Set data GPIOs from channel pins
        for (size_t i = 0; i < channelData.size() && i < 16; i++) {
            mConfig.data_gpios[i] = channelData[i]->getPin();
        }

        // Configure peripheral
        detail::I2sLcdCamConfig pconfig;
        pconfig.num_lanes = mConfig.num_lanes;
        pconfig.pclk_hz = mConfig.pclk_hz;
        pconfig.use_psram = mConfig.use_psram;

        // Calculate buffer size:
        // Each LED = 3 components (RGB)
        // Each component = 8 bits
        // Each bit = 3 words (high, data, low pattern)
        // Each word = 2 bytes (16-bit parallel output)
        // Plus offset at start and end for timing
        size_t offset_start = 0;
        size_t offset_end = 24 * 3 * 2 * 2 * 2 + 2;  // From Yves driver
        size_t data_size = mNumComponents * mNumLeds * 8 * 3 * 2 + offset_start + offset_end;
        pconfig.max_transfer_bytes = data_size;

        pconfig.data_gpios.resize(16);
        for (int i = 0; i < 16; i++) {
            pconfig.data_gpios[i] = mConfig.data_gpios[i];
        }

        if (!mPeripheral->initialize(pconfig)) {
            FL_WARN("ChannelEngineI2S: Failed to initialize peripheral");
            return false;
        }

        mBufferSize = data_size;

        // Allocate double buffers
        for (int i = 0; i < 2; i++) {
            mBuffers[i] = mPeripheral->allocateBuffer(mBufferSize);
            if (mBuffers[i] == nullptr) {
                FL_WARN("ChannelEngineI2S: Failed to allocate buffer");
                return false;
            }
            // Initialize with zeros
            fl::memset(mBuffers[i], 0, mBufferSize);
        }

        // Initialize fixed high bits in buffer pattern (from Yves driver)
        // The middle word of each 3-word bit pattern is always high
        for (int buf = 0; buf < 2; buf++) {
            uint16_t* output = mBuffers[buf];
            for (int i = 0; i < mNumLeds * mNumComponents * 8; i++) {
                output[3 * i + 1] = 0xFFFF;  // Middle word is always high
            }
        }

        mInitialized = true;
    }

    // Prepare scratch buffer
    prepareScratchBuffer(channelData, maxChannelSize);

    // Mark channels as in use
    for (const auto& channel : channelData) {
        channel->setInUse(true);
    }

    // Encode frame data
    encodeFrame();

    // Start DMA transfer
    mBusy = true;
    int backBuffer = 1 - mFrontBuffer;
    if (!mPeripheral->transmit(mBuffers[backBuffer], mBufferSize)) {
        mBusy = false;
        // Clean up on failure - mark channels as not in use
        for (const auto& channel : channelData) {
            channel->setInUse(false);
        }
        FL_WARN("ChannelEngineI2S: Failed to start transmission");
        return false;
    }

    mFrontBuffer = backBuffer;
    mFrameCounter++;
    return true;
}

void ChannelEngineI2S::prepareScratchBuffer(fl::span<const ChannelDataPtr> channelData,
                                            size_t maxChannelSize) {
    // Resize scratch buffer to hold all channel data
    size_t totalSize = channelData.size() * maxChannelSize;
    mScratchBuffer.resize(totalSize);

    // Zero-fill for unused bytes
    fl::memset(mScratchBuffer.data(), 0, totalSize);

    // Copy each channel's data to its lane
    for (size_t lane = 0; lane < channelData.size(); lane++) {
        const auto& channel = channelData[lane];
        const auto& data = channel->getData();
        size_t offset = lane * maxChannelSize;
        fl::memcpy(mScratchBuffer.data() + offset, data.data(), data.size());

        // Set strip pointer for encoding
        mStrips[lane] = reinterpret_cast<CRGB*>(mScratchBuffer.data() + offset); // ok reinterpret cast - scratch buffer layout matches CRGB alignment
    }
}

void ChannelEngineI2S::encodeFrame() {
    int backBuffer = 1 - mFrontBuffer;
    uint16_t* output = mBuffers[backBuffer];

    // Skip initial offset and the systematic first high word
    output += 2;

    // Process each LED
    uint8_t pixel_bytes[16];

    for (int led_idx = 0; led_idx < mNumLeds; led_idx++) {
        // Process each color component (RGB)
        for (int component = 0; component < mNumComponents; component++) {
            // Gather this component byte from all lanes
            for (int lane = 0; lane < mConfig.num_lanes; lane++) {
                if (mStrips[lane] != nullptr) {
                    // Access raw bytes: strip[led_idx].raw[component]
                    pixel_bytes[lane] = mStrips[lane][led_idx].raw[component];
                } else {
                    pixel_bytes[lane] = 0;
                }
            }

            // Fill unused lanes with zeros
            for (int lane = mConfig.num_lanes; lane < 16; lane++) {
                pixel_bytes[lane] = 0;
            }

            // Transpose and encode
            transpose16x1(pixel_bytes, output);
            output += 24;  // 8 bits × 3 words per bit = 24 words
        }
    }
}

//=============================================================================
// Factory Function
//=============================================================================

/// @brief Shared pointer wrapper for singleton peripheral
/// This wraps the singleton in a shared_ptr with a no-op deleter since
/// the singleton manages its own lifetime.
class I2sLcdCamPeripheralSingletonWrapper : public detail::II2sLcdCamPeripheral {
public:
    I2sLcdCamPeripheralSingletonWrapper(detail::II2sLcdCamPeripheral& impl) : mImpl(impl) {}

    bool initialize(const detail::I2sLcdCamConfig& config) override {
        return mImpl.initialize(config);
    }
    void deinitialize() override { mImpl.deinitialize(); }
    bool isInitialized() const override { return mImpl.isInitialized(); }
    uint16_t* allocateBuffer(size_t size_bytes) override {
        return mImpl.allocateBuffer(size_bytes);
    }
    void freeBuffer(uint16_t* buffer) override { mImpl.freeBuffer(buffer); }
    bool transmit(const uint16_t* buffer, size_t size_bytes) override {
        return mImpl.transmit(buffer, size_bytes);
    }
    bool waitTransmitDone(uint32_t timeout_ms) override {
        return mImpl.waitTransmitDone(timeout_ms);
    }
    bool isBusy() const override { return mImpl.isBusy(); }
    bool registerTransmitCallback(void* callback, void* user_ctx) override {
        return mImpl.registerTransmitCallback(callback, user_ctx);
    }
    const detail::I2sLcdCamConfig& getConfig() const override {
        return mImpl.getConfig();
    }
    uint64_t getMicroseconds() override { return mImpl.getMicroseconds(); }
    void delay(uint32_t ms) override { mImpl.delay(ms); }

private:
    detail::II2sLcdCamPeripheral& mImpl;
};

fl::shared_ptr<IChannelEngine> createI2sEngine() {
#if defined(CONFIG_IDF_TARGET_ESP32S3)
    // Wrap singleton in shared_ptr (singleton manages its own lifetime)
    auto wrapper = fl::make_shared<I2sLcdCamPeripheralSingletonWrapper>(
        detail::I2sLcdCamPeripheralEsp::instance()
    );
    return fl::make_shared<ChannelEngineI2S>(wrapper);
#else
    // No hardware available
    return nullptr;
#endif
}

} // namespace fl

// Undefine transpose constants
#undef AA
#undef CC
#undef FF
#undef FF2

#endif // CONFIG_IDF_TARGET_ESP32S3 || FASTLED_STUB_IMPL || host platform
