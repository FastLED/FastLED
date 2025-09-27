/// @file esp32s3_clockless_i2s.cpp
/// Implementation of ESP32-S3 LCD/I80 Parallel LED Driver

#include "esp32s3_clockless_i2s.h"
#include "chipsets.h"
#include "fl/math.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/i2s.h"

static const char* TAG = "LCD_LED_S3";

FASTLED_NAMESPACE_BEGIN

// Timing constants from chipsets.h
#ifndef FASTLED_WS2812_T1
#define FASTLED_WS2812_T1 250
#endif
#ifndef FASTLED_WS2812_T2  
#define FASTLED_WS2812_T2 625
#endif
#ifndef FASTLED_WS2812_T3
#define FASTLED_WS2812_T3 375
#endif

// WS2813/WS2811 use 320ns, 320ns, 640ns
#define FASTLED_WS2813_T1 320
#define FASTLED_WS2813_T2 320
#define FASTLED_WS2813_T3 640

#define FASTLED_WS2811_T1 320
#define FASTLED_WS2811_T2 320
#define FASTLED_WS2811_T3 640

// WS2816 uses similar timing to WS2812
#define FASTLED_WS2816_T1 250
#define FASTLED_WS2816_T2 625
#define FASTLED_WS2816_T3 375

LcdLedDriverS3::LcdLedDriverS3() 
    : mNBit(0)
    , mLedsPerStrip(0)
    , mBusHandle(nullptr)
    , mIOHandle(nullptr)
    , mDMABuffers{nullptr, nullptr}
    , mBufferSize(0)
    , mActiveBuffer(0)
    , mEncodeBuffer(1)
    , mTransferSemaphore(nullptr)
    , mTransferActive(false)
    , mSlotTimeNs(50)
    , mLatchSlots(0)
    , mFrameCount(0)
    , mLastFrameTime(0)
{
    // Initialize strip pointers to nullptr
    for (int i = 0; i < 16; i++) {
        mStrips[i] = nullptr;
    }
}

LcdLedDriverS3::~LcdLedDriverS3() {
    // Clean up resources
    if (mTransferSemaphore) {
        vSemaphoreDelete(mTransferSemaphore);
    }
    
    if (mIOHandle) {
        esp_lcd_panel_io_del(mIOHandle);
    }
    
    if (mBusHandle) {
        esp_lcd_i80_bus_del(mBusHandle);
    }
    
    // Free DMA buffers
    for (int i = 0; i < 2; i++) {
        if (mDMABuffers[i]) {
            heap_caps_free(mDMABuffers[i]);
        }
    }
}

uint32_t LcdLedDriverS3::ns_to_slots(uint32_t ns, uint32_t pclk_hz) {
    // Convert nanoseconds to slots with proper rounding
    // slot_time_ns = 1e9 / pclk_hz
    uint64_t slots = (uint64_t)ns * pclk_hz + 500000000ULL;
    slots /= 1000000000ULL;
    return (uint32_t)(slots ? slots : 1);  // Minimum 1 slot
}

void LcdLedDriverS3::getChipsetTiming(LedChipset chipset, uint32_t& T1, uint32_t& T2, uint32_t& T3) {
    switch (chipset) {
        case LedChipset::WS2812:
            T1 = FASTLED_WS2812_T1;
            T2 = FASTLED_WS2812_T2;
            T3 = FASTLED_WS2812_T3;
            break;
            
        case LedChipset::WS2816:
            T1 = FASTLED_WS2816_T1;
            T2 = FASTLED_WS2816_T2;
            T3 = FASTLED_WS2816_T3;
            break;
            
        case LedChipset::WS2813:
            T1 = FASTLED_WS2813_T1;
            T2 = FASTLED_WS2813_T2;
            T3 = FASTLED_WS2813_T3;
            break;
            
        case LedChipset::WS2811:
            T1 = FASTLED_WS2811_T1;
            T2 = FASTLED_WS2811_T2;
            T3 = FASTLED_WS2811_T3;
            break;
            
        case LedChipset::Custom:
        default:
            // Custom timing should be provided by caller
            T1 = T2 = T3 = 0;
            break;
    }
}

LaneTiming LcdLedDriverS3::computeLaneTiming(const LaneConfig& cfg, uint32_t pclk_hz) {
    LaneTiming timing;
    
    uint32_t T1, T2, T3;
    if (cfg.chipset == LedChipset::Custom) {
        T1 = cfg.T1_ns;
        T2 = cfg.T2_ns;
        T3 = cfg.T3_ns;
    } else {
        getChipsetTiming(cfg.chipset, T1, T2, T3);
    }
    
    // Convert to slots with proper rounding strategy
    // T1 and T3 use ceil to ensure minimum timing requirements
    // T2 uses round for balanced error distribution
    timing.S_T1 = ns_to_slots(T1, pclk_hz);
    timing.S_T2 = (ns_to_slots(T2 * 2, pclk_hz) + 1) / 2;  // Round to nearest
    timing.S_T3 = ns_to_slots(T3, pclk_hz);
    
    timing.N_lane = timing.S_T1 + timing.S_T2 + timing.S_T3;
    timing.HS0 = timing.S_T1;                    // High slots for bit 0
    timing.HS1 = timing.S_T1 + timing.S_T2;     // High slots for bit 1
    
    return timing;
}

bool LcdLedDriverS3::begin(const DriverConfig& cfg) {
    if (cfg.lanes.empty() || cfg.lanes.size() > 16) {
        ESP_LOGE(TAG, "Invalid lane count: %zu (must be 1-16)", cfg.lanes.size());
        return false;
    }
    
    if (cfg.queue_depth != 1) {
        ESP_LOGE(TAG, "Queue depth must be 1 for long frames, got %d", cfg.queue_depth);
        return false;
    }
    
    mConfig = cfg;
    mSlotTimeNs = 1000000000UL / cfg.pclk_hz;  // ns per slot
    mLatchSlots = ns_to_slots(cfg.latch_us * 1000, cfg.pclk_hz);
    
    // Compute lane timings
    mLaneTimings.clear();
    mNBit = 0;
    
    for (const auto& lane_cfg : cfg.lanes) {
        LaneTiming timing = computeLaneTiming(lane_cfg, cfg.pclk_hz);
        mLaneTimings.push_back(timing);
        
        // N_bit is the maximum across all lanes for harmonization
        if (timing.N_lane > mNBit) {
            mNBit = timing.N_lane;
        }
        
        ESP_LOGI(TAG, "Lane GPIO%d: T1=%uns(%u slots), T2=%uns(%u slots), T3=%uns(%u slots), Total=%u slots",
                lane_cfg.gpio,
                lane_cfg.chipset == LedChipset::Custom ? lane_cfg.T1_ns : 0, timing.S_T1,
                lane_cfg.chipset == LedChipset::Custom ? lane_cfg.T2_ns : 0, timing.S_T2,
                lane_cfg.chipset == LedChipset::Custom ? lane_cfg.T3_ns : 0, timing.S_T3,
                timing.N_lane);
    }
    
    ESP_LOGI(TAG, "Harmonized bit length: %u slots (%.1f µs)", mNBit, mNBit * mSlotTimeNs / 1000.0f);
    
    // Initialize I80 bus
    if (!initI80Bus()) {
        ESP_LOGE(TAG, "Failed to initialize I80 bus");
        return false;
    }
    
    // Create transfer completion semaphore
    mTransferSemaphore = xSemaphoreCreateBinary();
    if (!mTransferSemaphore) {
        ESP_LOGE(TAG, "Failed to create transfer semaphore");
        return false;
    }
    
    ESP_LOGI(TAG, "LCD LED driver initialized successfully");
    ESP_LOGI(TAG, "PCLK: %.1f MHz, Slot time: %u ns, Latch: %u µs (%u slots)",
             cfg.pclk_hz / 1000000.0f, mSlotTimeNs, cfg.latch_us, mLatchSlots);
    
    return true;
}

bool LcdLedDriverS3::initI80Bus() {
    // Prepare GPIO array for data pins
    int data_gpios[16];
    for (size_t i = 0; i < mConfig.lanes.size(); i++) {
        data_gpios[i] = mConfig.lanes[i].gpio;
    }
    // Fill unused pins with -1
    for (size_t i = mConfig.lanes.size(); i < 16; i++) {
        data_gpios[i] = -1;
    }
    
    // I80 bus configuration
    esp_lcd_i80_bus_config_t bus_config = {};
    bus_config.clk_src = LCD_CLK_SRC_DEFAULT;
    bus_config.dc_gpio_num = -1;        // Not used
    bus_config.wr_gpio_num = GPIO_NUM_47;  // Choose appropriate WR pin
    bus_config.data_gpio_nums = data_gpios;
    bus_config.bus_width = 16;
    bus_config.max_transfer_bytes = mConfig.use_psram ? (4 * 1024 * 1024) : (64 * 1024);
    bus_config.psram_trans_align = 64;
    bus_config.sram_trans_align = 4;
    
    esp_err_t ret = esp_lcd_new_i80_bus(&bus_config, &mBusHandle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I80 bus: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Panel IO configuration
    esp_lcd_panel_io_i80_config_t io_config = {};
    io_config.cs_gpio_num = -1;         // Not used
    io_config.pclk_hz = mConfig.pclk_hz;
    io_config.trans_queue_depth = mConfig.queue_depth;
    io_config.on_color_trans_done = onTransferDone;
    io_config.user_ctx = this;
    io_config.lcd_cmd_bits = 8;         // Dummy command
    io_config.lcd_param_bits = 8;       // Not used
    io_config.dc_levels.dc_idle_level = 0;
    io_config.dc_levels.dc_cmd_level = 0;
    io_config.dc_levels.dc_dummy_level = 0;
    io_config.dc_levels.dc_data_level = 1;
    
    ret = esp_lcd_new_panel_io_i80(mBusHandle, &io_config, &mIOHandle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create panel IO: %s", esp_err_to_name(ret));
        return false;
    }
    
    return true;
}

void LcdLedDriverS3::attachStrips(CRGB* strips[16], int leds_per_strip) {
    mLedsPerStrip = leds_per_strip;
    
    for (int i = 0; i < 16; i++) {
        mStrips[i] = strips[i];
    }
    
    // Allocate DMA buffers now that we know the frame size
    if (!allocateBuffers()) {
        ESP_LOGE(TAG, "Failed to allocate DMA buffers");
    }
}

bool LcdLedDriverS3::allocateBuffers() {
    if (mLedsPerStrip == 0 || mNBit == 0) {
        ESP_LOGE(TAG, "Cannot allocate buffers: LEDs=%d, N_bit=%u", mLedsPerStrip, mNBit);
        return false;
    }
    
    // Calculate buffer size
    // Per LED: 24 bits (GRB) * N_bit slots * 2 bytes per slot
    size_t frame_words = mLedsPerStrip * 24 * mNBit;
    size_t latch_words = mLatchSlots;
    mBufferSize = (frame_words + latch_words) * sizeof(uint16_t);
    
    ESP_LOGI(TAG, "Buffer size calculation:");
    ESP_LOGI(TAG, "  LEDs per strip: %d", mLedsPerStrip);
    ESP_LOGI(TAG, "  Bits per LED: 24 (GRB)");
    ESP_LOGI(TAG, "  Slots per bit: %u", mNBit);
    ESP_LOGI(TAG, "  Frame words: %zu", frame_words);
    ESP_LOGI(TAG, "  Latch words: %zu", latch_words);
    ESP_LOGI(TAG, "  Buffer size: %zu bytes (%.1f KB)", mBufferSize, mBufferSize / 1024.0f);
    
    // Allocate double buffers
    uint32_t caps = MALLOC_CAP_DMA;
    if (mConfig.use_psram) {
        caps |= MALLOC_CAP_SPIRAM;
    } else {
        caps |= MALLOC_CAP_INTERNAL;
    }
    
    for (int i = 0; i < 2; i++) {
        mDMABuffers[i] = (uint16_t*)heap_caps_aligned_alloc(64, mBufferSize, caps);
        if (!mDMABuffers[i]) {
            ESP_LOGE(TAG, "Failed to allocate DMA buffer %d (%zu bytes)", i, mBufferSize);
            return false;
        }
        
        // Initialize buffer to all zeros (all lanes LOW)
        memset(mDMABuffers[i], 0, mBufferSize);
    }
    
    ESP_LOGI(TAG, "Allocated %d DMA buffers of %zu bytes each (%s)",
             2, mBufferSize, mConfig.use_psram ? "PSRAM" : "Internal RAM");
    
    return true;
}

void LcdLedDriverS3::emitBitWords(uint16_t* out_words, uint16_t lane_bits) {
    // Clear the output words first
    memset(out_words, 0, mNBit * sizeof(uint16_t));
    
    // For each slot in the bit time
    for (uint32_t slot = 0; slot < mNBit; slot++) {
        uint16_t word = 0;
        
        // For each lane
        for (size_t lane = 0; lane < mConfig.lanes.size(); lane++) {
            const LaneTiming& timing = mLaneTimings[lane];
            bool bit_value = (lane_bits >> lane) & 1;
            bool should_be_high;
            
            if (bit_value) {
                // Bit 1: HIGH for HS1 slots
                should_be_high = (slot < timing.HS1);
            } else {
                // Bit 0: HIGH for HS0 slots  
                should_be_high = (slot < timing.HS0);
            }
            
            if (should_be_high) {
                word |= (1U << lane);
            }
        }
        
        out_words[slot] = word;
    }
}

void LcdLedDriverS3::encodeFrame(int buffer_index) {
    if (!mDMABuffers[buffer_index] || mLedsPerStrip == 0) {
        return;
    }
    
    uint16_t* buffer = mDMABuffers[buffer_index];
    uint16_t* write_ptr = buffer;
    
    // Encode each LED
    for (int led = 0; led < mLedsPerStrip; led++) {
        // GRB order (standard for most WS28xx)
        uint8_t colors[3];  // [G, R, B]
        
        // Extract color data from each active lane
        for (int color_idx = 0; color_idx < 3; color_idx++) {
            // Process each bit (MSB first)
            for (int bit = 7; bit >= 0; bit--) {
                uint16_t lane_bits = 0;
                
                // Gather bit data from all active lanes
                for (size_t lane = 0; lane < mConfig.lanes.size(); lane++) {
                    if (mStrips[lane] && led < mLedsPerStrip) {
                        uint8_t color_value;
                        switch (color_idx) {
                            case 0: color_value = mStrips[lane][led].g; break;  // Green
                            case 1: color_value = mStrips[lane][led].r; break;  // Red
                            case 2: color_value = mStrips[lane][led].b; break;  // Blue
                            default: color_value = 0; break;
                        }
                        
                        if (color_value & (1 << bit)) {
                            lane_bits |= (1U << lane);
                        }
                    }
                }
                
                // Emit bit words for this bit position
                emitBitWords(write_ptr, lane_bits);
                write_ptr += mNBit;
            }
        }
    }
    
    // Add latch gap (all lanes LOW)
    memset(write_ptr, 0, mLatchSlots * sizeof(uint16_t));
    
    ESP_LOGD(TAG, "Encoded frame %u in buffer %d (%zu words)",
             mFrameCount, buffer_index, write_ptr - buffer + mLatchSlots);
}

bool LcdLedDriverS3::show() {
    if (!mIOHandle || mTransferActive) {
        return false;  // Transfer already in progress
    }
    
    // Encode the frame into the back buffer
    encodeFrame(mEncodeBuffer);
    
    // Start DMA transfer
    mTransferActive = true;
    uint32_t start_time = esp_timer_get_time();
    
    esp_err_t ret = esp_lcd_panel_io_tx_color(mIOHandle, 0x2C,  // Dummy command
                                             mDMABuffers[mEncodeBuffer],
                                             mBufferSize);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start transfer: %s", esp_err_to_name(ret));
        mTransferActive = false;
        return false;
    }
    
    // Swap buffers for next frame
    mActiveBuffer = mEncodeBuffer;
    mEncodeBuffer = 1 - mEncodeBuffer;
    
    mFrameCount++;
    mLastFrameTime = start_time;
    
    return true;
}

bool LcdLedDriverS3::busy() const {
    return mTransferActive;
}

void LcdLedDriverS3::wait() {
    if (mTransferActive && mTransferSemaphore) {
        xSemaphoreTake(mTransferSemaphore, portMAX_DELAY);
    }
}

bool IRAM_ATTR LcdLedDriverS3::onTransferDone(esp_lcd_panel_io_handle_t panel_io,
                                              esp_lcd_panel_io_event_data_t* edata,
                                              void* user_ctx) {
    LcdLedDriverS3* driver = static_cast<LcdLedDriverS3*>(user_ctx);
    
    // Mark transfer as complete
    driver->mTransferActive = false;
    
    // Signal completion
    BaseType_t higher_priority_task_woken = pdFALSE;
    xSemaphoreGiveFromISR(driver->mTransferSemaphore, &higher_priority_task_woken);
    
    return higher_priority_task_woken == pdTRUE;
}

void LcdLedDriverS3::setLaneChipset(int lane, LedChipset chipset) {
    if (lane < 0 || lane >= (int)mConfig.lanes.size()) {
        return;
    }
    
    mConfig.lanes[lane].chipset = chipset;
    
    // Recompute timing for this lane
    mLaneTimings[lane] = computeLaneTiming(mConfig.lanes[lane], mConfig.pclk_hz);
    
    // Recompute N_bit (may have changed)
    mNBit = 0;
    for (const auto& timing : mLaneTimings) {
        if (timing.N_lane > mNBit) {
            mNBit = timing.N_lane;
        }
    }
}

void LcdLedDriverS3::setLaneTimings(int lane, uint32_t T1, uint32_t T2, uint32_t T3) {
    if (lane < 0 || lane >= (int)mConfig.lanes.size()) {
        return;
    }
    
    mConfig.lanes[lane].chipset = LedChipset::Custom;
    mConfig.lanes[lane].T1_ns = T1;
    mConfig.lanes[lane].T2_ns = T2;
    mConfig.lanes[lane].T3_ns = T3;
    
    // Recompute timing for this lane
    mLaneTimings[lane] = computeLaneTiming(mConfig.lanes[lane], mConfig.pclk_hz);
    
    // Recompute N_bit (may have changed)
    mNBit = 0;
    for (const auto& timing : mLaneTimings) {
        if (timing.N_lane > mNBit) {
            mNBit = timing.N_lane;
        }
    }
}

float LcdLedDriverS3::getMaxFrameRate() const {
    if (mLedsPerStrip == 0 || mNBit == 0) {
        return 0.0f;
    }
    
    // Frame time = LEDs * 24 bits * N_bit slots * slot_time + latch_time
    uint32_t frame_time_us = mLedsPerStrip * 24 * mNBit * mSlotTimeNs / 1000 + mConfig.latch_us;
    return 1000000.0f / frame_time_us;
}

size_t LcdLedDriverS3::getMemoryUsage() const {
    return mBufferSize * 2;  // Double buffered
}

FASTLED_NAMESPACE_END