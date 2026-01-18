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
#include "wave8_encoder_i2s.h"
#include "fl/warn.h"
#include "fl/dbg.h"
#include "fl/stl/cstring.h"
#include "fl/channels/wave8.h"
#include "fl/channels/detail/wave8.hpp"

// Include ESP implementation only on real hardware
#if defined(CONFIG_IDF_TARGET_ESP32S3)
#include "i2s_lcd_cam_peripheral_esp.h"
#endif

namespace fl {

// Use wave8 encoding by default
#ifndef FASTLED_I2S_USE_WAVE8
#define FASTLED_I2S_USE_WAVE8 1
#endif

//=============================================================================
// Transpose Constants (from Yves driver) - kept for fallback
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
      mFrameCounter(0),
      mWave8Lut(),
      mWave8LutValid(false),
      mCurrentTiming(0, 0, 0, 0, "UNINITIALIZED") {
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
    // WS2812 encoding: 3-word pattern per bit
    //   Word 0 (indices 0,3,6,...): Always HIGH (start of pulse)
    //   Word 1 (indices 1,4,7,...): Data-dependent (HIGH=1, LOW=0)
    //   Word 2 (indices 2,5,8,...): Always LOW (end of pulse)
    //
    // For bit 0: HIGH-LOW-LOW = short pulse (~417ns HIGH, ~833ns LOW)
    // For bit 1: HIGH-HIGH-LOW = long pulse (~833ns HIGH, ~417ns LOW)
    //
    // Transposed data goes in middle words (indices 1,4,7,10,13,16,19,22)
    B[1] = static_cast<uint16_t>(((x & 0xff000000) >> 8 | ((x1 & 0xff000000))) >> 16);
    B[4] = static_cast<uint16_t>(((x & 0xff0000) >> 16 | ((x1 & 0xff0000) >> 8)));
    B[7] = static_cast<uint16_t>(((x & 0xff00) | ((x1 & 0xff00) << 8)) >> 8);
    B[10] = static_cast<uint16_t>((x & 0xff) | ((x1 & 0xff) << 8));
    B[13] = static_cast<uint16_t>(((y & 0xff000000) >> 8 | ((y1 & 0xff000000))) >> 16);
    B[16] = static_cast<uint16_t>(((y & 0xff0000) | ((y1 & 0xff0000) << 8)) >> 16);
    B[19] = static_cast<uint16_t>(((y & 0xff00) | ((y1 & 0xff00) << 8)) >> 8);
    B[22] = static_cast<uint16_t>((y & 0xff) | ((y1 & 0xff) << 8));
}

//=============================================================================
// Transmission Implementation
//=============================================================================

bool ChannelEngineI2S::beginTransmission(fl::span<const ChannelDataPtr> channelData) {
    if (channelData.empty() || !mPeripheral) {
        return false;
    }

    // Get timing from first channel
    auto timing = channelData[0]->getTiming();

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

    // Check if we need to rebuild the Wave8 LUT
    bool needsLutRebuild = !mWave8LutValid || !(mCurrentTiming == timing);
    if (needsLutRebuild) {
        // Build Wave8 LUT from chipset timing
        ChipsetTiming ct;
        ct.T1 = timing.t1_ns;
        ct.T2 = timing.t2_ns;
        ct.T3 = timing.t3_ns;
        mWave8Lut = buildWave8ExpansionLUT(ct);
        mWave8LutValid = true;
        mCurrentTiming = timing;

        // Calculate required I2S clock frequency for wave8 encoding
        uint32_t clock_hz = calculateI2sClockHz(ct);
        mConfig.pclk_hz = clock_hz;

        FL_DBG("ChannelEngineI2S: Built Wave8 LUT for timing T1=" << timing.t1_ns
               << "ns, T2=" << timing.t2_ns << "ns, T3=" << timing.t3_ns << "ns");
        FL_DBG("ChannelEngineI2S: I2S clock set to " << clock_hz << " Hz");
    }

    // Initialize or reconfigure if needed
    bool needsInit = !mInitialized ||
                     mNumLeds != numLeds ||
                     mConfig.num_lanes != numLanes ||
                     needsLutRebuild;

    if (needsInit) {
        // Free old buffers
        for (int i = 0; i < 2; i++) {
            if (mBuffers[i] != nullptr) {
                mPeripheral->freeBuffer(mBuffers[i]);
                mBuffers[i] = nullptr;
            }
        }

        // Deinitialize peripheral if timing changed
        if (mInitialized && needsLutRebuild) {
            mPeripheral->deinitialize();
            mInitialized = false;
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

#if FASTLED_I2S_USE_WAVE8
        // Wave8 encoding: Each LED byte → 8 pulses → 8 × 16-bit words
        // Each LED = 3 bytes (RGB)
        // Each byte = 64 bits of output (8 Wave8Bit × 8 bits each)
        // Each bit output as one 16-bit word
        // Buffer size = numLeds * 3 * 64 * 2 bytes = numLeds * 384 bytes
        // Plus padding for reset signal
        size_t reset_words = 64;  // ~50µs reset at typical clock
        size_t data_words = static_cast<size_t>(mNumLeds) * mNumComponents * 64;
        size_t total_words = data_words + reset_words;
        size_t data_size = total_words * sizeof(uint16_t);
        pconfig.max_transfer_bytes = data_size;
        mBufferSize = data_size;

        FL_DBG("ChannelEngineI2S: Wave8 buffer size = " << data_size << " bytes ("
               << total_words << " words) for " << mNumLeds << " LEDs");
#else
        // Legacy transpose encoding
        size_t offset_start = 0;
        size_t offset_end = 24 * 3 * 2 * 2 * 2 + 2;
        size_t data_size = mNumComponents * mNumLeds * 8 * 3 * 2 + offset_start + offset_end;
        pconfig.max_transfer_bytes = data_size;
        mBufferSize = data_size;
#endif

        pconfig.data_gpios.resize(16);
        for (int i = 0; i < 16; i++) {
            pconfig.data_gpios[i] = mConfig.data_gpios[i];
        }

        if (!mPeripheral->initialize(pconfig)) {
            FL_WARN("ChannelEngineI2S: Failed to initialize peripheral");
            return false;
        }

        // Allocate double buffers
        for (int i = 0; i < 2; i++) {
            mBuffers[i] = mPeripheral->allocateBuffer(mBufferSize);
            if (mBuffers[i] == nullptr) {
                FL_WARN("ChannelEngineI2S: Failed to allocate buffer");
                return false;
            }
            // Initialize with zeros (LOW for reset)
            fl::memset(mBuffers[i], 0, mBufferSize);
        }

#if !FASTLED_I2S_USE_WAVE8
        // Initialize fixed high bits in buffer pattern for legacy transpose encoding
        for (int buf = 0; buf < 2; buf++) {
            uint16_t* output = mBuffers[buf];
            for (int i = 0; i < mNumLeds * mNumComponents * 8; i++) {
                output[3 * i + 0] = 0xFFFF;
            }
        }
#endif

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

#if FASTLED_I2S_USE_WAVE8
    // Wave8 encoding: Convert each LED byte to 64 output bits
    // For single lane: output bits go on D0
    // For multi-lane: output bits interleaved across D0-D15

    size_t output_idx = 0;

    // Process each LED
    for (int led_idx = 0; led_idx < mNumLeds; led_idx++) {
        // Process each color component (RGB)
        for (int component = 0; component < mNumComponents; component++) {
            // Gather this component byte from all lanes
            Wave8Byte wave8_lanes[16];

            for (int lane = 0; lane < mConfig.num_lanes; lane++) {
                uint8_t byte_val = 0;
                if (mStrips[lane] != nullptr) {
                    byte_val = mStrips[lane][led_idx].raw[component];
                }
                // Convert byte to Wave8Byte using LUT
                detail::wave8_convert_byte_to_wave8byte(byte_val, mWave8Lut, &wave8_lanes[lane]);
            }

            // Zero unused lanes
            for (int lane = mConfig.num_lanes; lane < 16; lane++) {
                fl::memset(&wave8_lanes[lane], 0, sizeof(Wave8Byte));
            }

            // Output 64 bits (8 Wave8Bit symbols × 8 pulses each)
            // For each symbol (represents one bit of the original byte):
            for (int sym = 0; sym < 8; sym++) {
                // For each pulse within the symbol (MSB first):
                for (int pulse = 7; pulse >= 0; pulse--) {
                    uint16_t word = 0;

                    // Gather pulse bit from each lane
                    for (int lane = 0; lane < mConfig.num_lanes; lane++) {
                        uint8_t wave8_bits = wave8_lanes[lane].symbols[sym].data;
                        uint16_t bit = (wave8_bits >> pulse) & 1;
                        word |= (bit << lane);
                    }

                    output[output_idx++] = word;
                }
            }
        }
    }

    // Rest of buffer is already zeros (reset signal)
#else
    // Legacy transpose encoding
    // The initialization already set output[0,3,6,...] to 0xFFFF
    // transpose16x1 writes data to output[1,4,7,...] (middle words)
    // output[2,5,8,...] remain 0 (last words, already zeros from memset)

    uint8_t pixel_bytes[16];

    for (int led_idx = 0; led_idx < mNumLeds; led_idx++) {
        // Process each color component (RGB)
        for (int component = 0; component < mNumComponents; component++) {
            // Gather this component byte from all lanes
            for (int lane = 0; lane < mConfig.num_lanes; lane++) {
                if (mStrips[lane] != nullptr) {
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
#endif
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
