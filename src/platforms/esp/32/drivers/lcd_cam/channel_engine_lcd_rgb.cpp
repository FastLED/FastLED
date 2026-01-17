/// @file channel_engine_lcd_rgb.cpp
/// @brief LCD RGB channel engine implementation for ESP32-P4

// Compile for:
// 1. ESP32-P4 with RGB LCD support (real hardware)
// 2. Stub/host platform testing (FASTLED_STUB_IMPL or host OS)
#if defined(ESP32)
#include "sdkconfig.h"
#endif

#if (defined(CONFIG_IDF_TARGET_ESP32P4) && __has_include("esp_lcd_panel_rgb.h")) || \
    defined(FASTLED_STUB_IMPL) || \
    (!defined(ARDUINO) && (defined(__linux__) || defined(__APPLE__) || defined(_WIN32)))

#include "channel_engine_lcd_rgb.h"
#include "fl/warn.h"
#include "fl/stl/cstring.h"

// Include ESP implementation only on real hardware
#if defined(CONFIG_IDF_TARGET_ESP32P4) && __has_include("esp_lcd_panel_rgb.h")
#include "lcd_rgb_peripheral_esp.h"
#endif

namespace fl {

//=============================================================================
// Constructor / Destructor
//=============================================================================

ChannelEngineLcdRgb::ChannelEngineLcdRgb(fl::shared_ptr<detail::ILcdRgbPeripheral> peripheral)
    : mPeripheral(fl::move(peripheral)),
      mInitialized(false),
      mConfig(),
      mNumLeds(0),
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

ChannelEngineLcdRgb::~ChannelEngineLcdRgb() {
    // Wait for pending transmission
    while (mBusy) {
        poll();
    }

    // Free buffers
    if (mPeripheral) {
        for (int i = 0; i < 2; i++) {
            if (mBuffers[i] != nullptr) {
                mPeripheral->freeFrameBuffer(mBuffers[i]);
                mBuffers[i] = nullptr;
            }
        }
    }
}

//=============================================================================
// IChannelEngine Interface
//=============================================================================

void ChannelEngineLcdRgb::enqueue(ChannelDataPtr channelData) {
    mEnqueuedChannels.push_back(fl::move(channelData));
}

void ChannelEngineLcdRgb::show() {
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

IChannelEngine::EngineState ChannelEngineLcdRgb::poll() {
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
// Transmission Implementation
//=============================================================================

bool ChannelEngineLcdRgb::beginTransmission(fl::span<const ChannelDataPtr> channelData) {
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
                mPeripheral->freeFrameBuffer(mBuffers[i]);
                mBuffers[i] = nullptr;
            }
        }

        // Configure
        mNumLeds = numLeds;
        mConfig.num_lanes = numLanes;
        mConfig.pclk_gpio = 10;  // Default PCLK GPIO - should be configurable
        mConfig.use_psram = true;

        // Set data GPIOs from channel pins
        for (size_t i = 0; i < channelData.size() && i < 16; i++) {
            mConfig.data_gpios[i] = channelData[i]->getPin();
        }

        // Configure peripheral
        detail::LcdRgbPeripheralConfig pconfig;
        pconfig.pclk_gpio = mConfig.pclk_gpio;
        pconfig.vsync_gpio = -1;
        pconfig.hsync_gpio = -1;
        pconfig.de_gpio = -1;
        pconfig.disp_gpio = -1;
        pconfig.pclk_hz = 3200000;  // 3.2 MHz for WS2812 timing
        pconfig.num_lanes = mConfig.num_lanes;
        pconfig.h_res = mNumLeds * 24 * 4;  // 4 pixels per bit
        pconfig.v_res = 1;
        pconfig.vsync_front_porch = 0;
        pconfig.use_psram = mConfig.use_psram;

        pconfig.data_gpios.resize(16);
        for (int i = 0; i < 16; i++) {
            pconfig.data_gpios[i] = mConfig.data_gpios[i];
        }

        if (!mPeripheral->initialize(pconfig)) {
            FL_WARN("ChannelEngineLcdRgb: Failed to initialize peripheral");
            return false;
        }

        // Calculate buffer size
        size_t data_size = mNumLeds * 24 * 4 * 2;  // 4 pixels per bit, 2 bytes per pixel
        mBufferSize = data_size;

        // Allocate double buffers
        for (int i = 0; i < 2; i++) {
            mBuffers[i] = mPeripheral->allocateFrameBuffer(mBufferSize);
            if (mBuffers[i] == nullptr) {
                FL_WARN("ChannelEngineLcdRgb: Failed to allocate buffer");
                return false;
            }
            // Initialize with zeros
            for (size_t j = 0; j < mBufferSize / 2; j++) {
                mBuffers[i][j] = 0;
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
    if (!mPeripheral->drawFrame(mBuffers[backBuffer], mBufferSize)) {
        mBusy = false;
        // Clean up on failure - mark channels as not in use
        for (const auto& channel : channelData) {
            channel->setInUse(false);
        }
        FL_WARN("ChannelEngineLcdRgb: Failed to start transmission");
        return false;
    }

    mFrontBuffer = backBuffer;
    mFrameCounter++;
    return true;
}

void ChannelEngineLcdRgb::prepareScratchBuffer(fl::span<const ChannelDataPtr> channelData,
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

void ChannelEngineLcdRgb::encodeFrame() {
    int backBuffer = 1 - mFrontBuffer;
    uint16_t* output = mBuffers[backBuffer];

    // 4-pixel encoding templates
    // Bit 0: [HI, LO, LO, LO]
    // Bit 1: [HI, HI, LO, LO]
    constexpr uint16_t templateBit0[4] = {0xFFFF, 0x0000, 0x0000, 0x0000};
    constexpr uint16_t templateBit1[4] = {0xFFFF, 0xFFFF, 0x0000, 0x0000};

    // Encode all LEDs
    for (int led_idx = 0; led_idx < mNumLeds; led_idx++) {
        // GRB order
        const int color_order[3] = {1, 0, 2};

        for (int color = 0; color < 3; color++) {
            int component = color_order[color];

            // Gather bytes across all lanes
            uint8_t pixel_bytes[16];
            for (int lane = 0; lane < mConfig.num_lanes; lane++) {
                if (mStrips[lane] != nullptr) {
                    pixel_bytes[lane] = mStrips[lane][led_idx].raw[component];
                } else {
                    pixel_bytes[lane] = 0;
                }
            }

            // Fill unused lanes
            for (int lane = mConfig.num_lanes; lane < 16; lane++) {
                pixel_bytes[lane] = 0;
            }

            // Transpose and encode bits (MSB first)
            for (int bit_idx = 7; bit_idx >= 0; bit_idx--) {
                // Build lane mask from bit at bit_idx
                uint16_t mask = 0;
                for (int lane = 0; lane < 16; lane++) {
                    if (pixel_bytes[lane] & (1 << bit_idx)) {
                        mask |= (1 << lane);
                    }
                }

                // Apply templates based on mask
                for (int pixel = 0; pixel < 4; pixel++) {
                    output[pixel] = (templateBit0[pixel] & ~mask) |
                                    (templateBit1[pixel] & mask);
                }

                output += 4;
            }
        }
    }
}

//=============================================================================
// Factory Function
//=============================================================================

/// @brief Shared pointer wrapper for singleton peripheral
/// This wraps the singleton in a shared_ptr with a no-op deleter since
/// the singleton manages its own lifetime.
class LcdRgbPeripheralSingletonWrapper : public detail::ILcdRgbPeripheral {
public:
    LcdRgbPeripheralSingletonWrapper(detail::ILcdRgbPeripheral& impl) : mImpl(impl) {}

    bool initialize(const detail::LcdRgbPeripheralConfig& config) override {
        return mImpl.initialize(config);
    }
    void deinitialize() override { mImpl.deinitialize(); }
    bool isInitialized() const override { return mImpl.isInitialized(); }
    uint16_t* allocateFrameBuffer(size_t size_bytes) override {
        return mImpl.allocateFrameBuffer(size_bytes);
    }
    void freeFrameBuffer(uint16_t* buffer) override { mImpl.freeFrameBuffer(buffer); }
    bool drawFrame(const uint16_t* buffer, size_t size_bytes) override {
        return mImpl.drawFrame(buffer, size_bytes);
    }
    bool waitFrameDone(uint32_t timeout_ms) override {
        return mImpl.waitFrameDone(timeout_ms);
    }
    bool isBusy() const override { return mImpl.isBusy(); }
    bool registerDrawCallback(void* callback, void* user_ctx) override {
        return mImpl.registerDrawCallback(callback, user_ctx);
    }
    const detail::LcdRgbPeripheralConfig& getConfig() const override {
        return mImpl.getConfig();
    }
    uint64_t getMicroseconds() override { return mImpl.getMicroseconds(); }
    void delay(uint32_t ms) override { mImpl.delay(ms); }

private:
    detail::ILcdRgbPeripheral& mImpl;
};

fl::shared_ptr<IChannelEngine> createLcdRgbEngine() {
#if defined(CONFIG_IDF_TARGET_ESP32P4) && __has_include("esp_lcd_panel_rgb.h")
    // Wrap singleton in shared_ptr (singleton manages its own lifetime)
    auto wrapper = fl::make_shared<LcdRgbPeripheralSingletonWrapper>(
        detail::LcdRgbPeripheralEsp::instance()
    );
    return fl::make_shared<ChannelEngineLcdRgb>(wrapper);
#else
    // No hardware available
    return nullptr;
#endif
}

} // namespace fl

#endif // CONFIG_IDF_TARGET_ESP32P4 || FASTLED_STUB_IMPL || host platform
