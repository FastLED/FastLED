#pragma once

/// @file esp32s3_clockless_i2s.h
/// ESP32-S3 LCD/I80 Parallel LED Driver with 50ns timing resolution
/// 
/// This driver uses the ESP32-S3's LCD_CAM peripheral in I80 mode to drive
/// up to 16 WS28xx LED strips in parallel with precise 50ns timing slots.
/// 
/// Key Features:
/// - Multi-chipset support (WS2812, WS2816, WS2813, etc.) in same frame
/// - 50ns timing resolution (20MHz PCLK)
/// - Up to 16 parallel lanes
/// - Double-buffered PSRAM operation
/// - Uses FastLED's canonical timing definitions from chipsets.h
/// 
/// Design Document: src/third_party/yves/esp32s3_lcd_parallel_design.md

#include "sdkconfig.h"

#ifndef CONFIG_IDF_TARGET_ESP32S3
#error "This driver is only compatible with ESP32-S3"
#endif

#include "cpixel_ledcontroller.h"
#include "pixel_iterator.h"
#include "fl/vector.h"
#include "fl/optional.h"
#include "eorder.h"
#include "platforms/esp/esp_version.h"

// ESP-LCD includes
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

FASTLED_NAMESPACE_BEGIN

/// @brief Supported LED chipset types for per-lane configuration
enum class LedChipset {
    WS2812,     ///< WS2812 timing (T1=250ns, T2=625ns, T3=375ns)
    WS2816,     ///< WS2816 timing (similar to WS2812)
    WS2813,     ///< WS2813 timing (T1=320ns, T2=320ns, T3=640ns)
    WS2811,     ///< WS2811 timing (T1=320ns, T2=320ns, T3=640ns)
    Custom      ///< Custom timing specified per lane
};

/// @brief Configuration for a single LED lane
struct LaneConfig {
    int gpio;                    ///< GPIO pin for this lane (D0-D15)
    LedChipset chipset;         ///< Chipset type for timing
    uint32_t T1_ns;             ///< Custom T1 timing (ns) - only used if chipset == Custom
    uint32_t T2_ns;             ///< Custom T2 timing (ns) - only used if chipset == Custom  
    uint32_t T3_ns;             ///< Custom T3 timing (ns) - only used if chipset == Custom
    
    /// @brief Constructor with chipset selection
    LaneConfig(int pin, LedChipset chip) : gpio(pin), chipset(chip), T1_ns(0), T2_ns(0), T3_ns(0) {}
    
    /// @brief Constructor with custom timing
    LaneConfig(int pin, uint32_t t1, uint32_t t2, uint32_t t3) 
        : gpio(pin), chipset(LedChipset::Custom), T1_ns(t1), T2_ns(t2), T3_ns(t3) {}
};

/// @brief Driver configuration structure
struct DriverConfig {
    fl::vector<LaneConfig> lanes;    ///< Lane configurations (1-16 lanes)
    uint32_t pclk_hz;               ///< PCLK frequency (default: 20MHz for 50ns slots)
    uint32_t latch_us;              ///< Reset/latch gap duration (microseconds)
    bool use_psram;                 ///< Use PSRAM for DMA buffers
    int queue_depth;                ///< Transaction queue depth (must be 1)
    
    /// @brief Default constructor
    DriverConfig() : pclk_hz(20000000), latch_us(300), use_psram(true), queue_depth(1) {}
};

/// @brief Internal timing parameters for a lane
struct LaneTiming {
    uint32_t S_T1;      ///< T1 in 50ns slots
    uint32_t S_T2;      ///< T2 in 50ns slots  
    uint32_t S_T3;      ///< T3 in 50ns slots
    uint32_t N_lane;    ///< Total slots per bit for this lane
    uint32_t HS0;       ///< High slots for bit 0 (S_T1)
    uint32_t HS1;       ///< High slots for bit 1 (S_T1 + S_T2)
};

/// @brief ESP32-S3 LCD/I80 Parallel LED Driver
/// 
/// This class implements a high-performance parallel LED driver using the
/// ESP32-S3's LCD_CAM peripheral in I80 mode. It can drive up to 16 LED
/// strips simultaneously with precise timing control.
/// 
/// Usage:
/// @code
/// DriverConfig config;
/// config.lanes.push_back(LaneConfig(GPIO_NUM_1, LedChipset::WS2812));
/// config.lanes.push_back(LaneConfig(GPIO_NUM_2, LedChipset::WS2816));
/// 
/// LcdLedDriverS3 driver;
/// driver.begin(config);
/// 
/// CRGB strips[2][100];
/// CRGB* strip_ptrs[16] = {strips[0], strips[1], nullptr, ...};
/// driver.attachStrips(strip_ptrs, 100);
/// 
/// // Update LED data
/// driver.show();
/// @endcode
class LcdLedDriverS3 {
public:
    /// @brief Constructor
    LcdLedDriverS3();
    
    /// @brief Destructor
    ~LcdLedDriverS3();
    
    /// @brief Initialize the driver with configuration
    /// @param cfg Driver configuration
    /// @return true if initialization successful
    bool begin(const DriverConfig& cfg);
    
    /// @brief Attach LED strip arrays to the driver
    /// @param strips Array of pointers to CRGB arrays (one per lane, nullptr for unused lanes)
    /// @param leds_per_strip Number of LEDs per strip
    void attachStrips(CRGB* strips[16], int leds_per_strip);
    
    /// @brief Start LED update (encode and transmit)
    /// @return true if update started successfully
    bool show();
    
    /// @brief Check if DMA transfer is in progress
    /// @return true if busy
    bool busy() const;
    
    /// @brief Wait for current transfer to complete
    void wait();
    
    /// @brief Set chipset for a specific lane
    /// @param lane Lane index (0-15)
    /// @param chipset New chipset type
    void setLaneChipset(int lane, LedChipset chipset);
    
    /// @brief Set custom timing for a specific lane
    /// @param lane Lane index (0-15)
    /// @param T1 T1 timing in nanoseconds
    /// @param T2 T2 timing in nanoseconds
    /// @param T3 T3 timing in nanoseconds
    void setLaneTimings(int lane, uint32_t T1, uint32_t T2, uint32_t T3);
    
    /// @brief Get current frame rate capability
    /// @return Maximum theoretical frame rate (Hz)
    float getMaxFrameRate() const;
    
    /// @brief Get memory usage information
    /// @return Total memory used for DMA buffers (bytes)
    size_t getMemoryUsage() const;

private:
    /// @brief Convert nanoseconds to 50ns slots
    /// @param ns Nanoseconds
    /// @param pclk_hz PCLK frequency
    /// @return Number of slots
    static uint32_t ns_to_slots(uint32_t ns, uint32_t pclk_hz);
    
    /// @brief Get timing parameters for a chipset
    /// @param chipset Chipset type
    /// @param T1 Output T1 timing (ns)
    /// @param T2 Output T2 timing (ns)
    /// @param T3 Output T3 timing (ns)
    static void getChipsetTiming(LedChipset chipset, uint32_t& T1, uint32_t& T2, uint32_t& T3);
    
    /// @brief Compute timing parameters for a lane
    /// @param cfg Lane configuration
    /// @param pclk_hz PCLK frequency
    /// @return Lane timing parameters
    LaneTiming computeLaneTiming(const LaneConfig& cfg, uint32_t pclk_hz);
    
    /// @brief Initialize I80 bus and panel IO
    /// @return true if successful
    bool initI80Bus();
    
    /// @brief Allocate DMA buffers
    /// @return true if successful
    bool allocateBuffers();
    
    /// @brief Encode LED data into DMA buffer
    /// @param buffer_index Buffer index (0 or 1)
    void encodeFrame(int buffer_index);
    
    /// @brief Emit bit data for all lanes
    /// @param out_words Output word array (length N_bit)
    /// @param lane_bits Bit data for each lane (16 bits)
    void emitBitWords(uint16_t* out_words, uint16_t lane_bits);
    
    /// @brief DMA transfer completion callback
    /// @param panel_io Panel IO handle
    /// @param edata Event data
    /// @param user_ctx User context (this pointer)
    /// @return true to continue
    static bool IRAM_ATTR onTransferDone(esp_lcd_panel_io_handle_t panel_io,
                                         esp_lcd_panel_io_event_data_t* edata,
                                         void* user_ctx);
    
    // Configuration
    DriverConfig mConfig;                    ///< Driver configuration
    fl::vector<LaneTiming> mLaneTimings;    ///< Per-lane timing parameters
    uint32_t mNBit;                         ///< Slots per bit (max across all lanes)
    
    // Strip data
    CRGB* mStrips[16];                      ///< Pointers to LED strip data
    int mLedsPerStrip;                      ///< Number of LEDs per strip
    
    // I80/LCD handles
    esp_lcd_i80_bus_handle_t mBusHandle;    ///< I80 bus handle
    esp_lcd_panel_io_handle_t mIOHandle;    ///< Panel IO handle
    
    // DMA buffers
    uint16_t* mDMABuffers[2];               ///< Double DMA buffers
    size_t mBufferSize;                     ///< Size of each buffer (bytes)
    int mActiveBuffer;                      ///< Currently transmitting buffer
    int mEncodeBuffer;                      ///< Currently encoding buffer
    
    // Synchronization
    SemaphoreHandle_t mTransferSemaphore;   ///< Transfer completion semaphore
    volatile bool mTransferActive;          ///< Transfer in progress flag
    
    // Timing
    uint32_t mSlotTimeNs;                   ///< Slot duration in nanoseconds
    uint32_t mLatchSlots;                   ///< Latch gap in slots
    
    // Statistics
    mutable uint32_t mFrameCount;           ///< Frame counter
    mutable uint32_t mLastFrameTime;        ///< Last frame timestamp
};

/// @brief FastLED controller wrapper for single-lane operation
/// @tparam DATA_PIN GPIO pin number
/// @tparam RGB_ORDER Color order (GRB, RGB, etc.)
/// @tparam CHIPSET LED chipset type
template<int DATA_PIN, EOrder RGB_ORDER = GRB, LedChipset CHIPSET = LedChipset::WS2812>
class ClocklessController_LCD_ESP32S3 : public CPixelLEDController<RGB_ORDER> {
private:
    typedef CPixelLEDController<RGB_ORDER> Base;
    static LcdLedDriverS3 sDriver;          ///< Shared driver instance
    static bool sDriverInitialized;        ///< Driver initialization flag
    static int sLaneCount;                  ///< Number of active lanes
    
public:
    /// @brief Constructor
    ClocklessController_LCD_ESP32S3() {
        if (!sDriverInitialized) {
            initializeDriver();
        }
    }
    
    /// @brief Initialize the controller
    virtual void init() override {
        // Driver initialization handled in constructor
    }
    
    /// @brief Get maximum refresh rate
    /// @return Maximum refresh rate in Hz
    virtual uint16_t getMaxRefreshRate() const override {
        return static_cast<uint16_t>(sDriver.getMaxFrameRate());
    }

protected:
    /// @brief Begin LED update
    /// @param nleds Number of LEDs
    /// @return Data pointer (unused)
    virtual void* beginShowLeds(int nleds) override {
        return Base::beginShowLeds(nleds);
    }
    
    /// @brief Send pixel data
    /// @param pixels Pixel controller
    virtual void showPixels(PixelController<RGB_ORDER>& pixels) override {
        // Convert pixel data to CRGB array and update driver
        // Implementation will depend on integration with existing FastLED patterns
    }
    
    /// @brief End LED update
    /// @param data Data pointer (unused)
    virtual void endShowLeds(void* data) override {
        sDriver.show();
        Base::endShowLeds(data);
    }

private:
    /// @brief Initialize shared driver instance
    static void initializeDriver() {
        if (sDriverInitialized) return;
        
        DriverConfig config;
        config.lanes.push_back(LaneConfig(DATA_PIN, CHIPSET));
        
        if (sDriver.begin(config)) {
            sDriverInitialized = true;
            sLaneCount = 1;
        }
    }
};

// Static member definitions
template<int DATA_PIN, EOrder RGB_ORDER, LedChipset CHIPSET>
LcdLedDriverS3 ClocklessController_LCD_ESP32S3<DATA_PIN, RGB_ORDER, CHIPSET>::sDriver;

template<int DATA_PIN, EOrder RGB_ORDER, LedChipset CHIPSET>
bool ClocklessController_LCD_ESP32S3<DATA_PIN, RGB_ORDER, CHIPSET>::sDriverInitialized = false;

template<int DATA_PIN, EOrder RGB_ORDER, LedChipset CHIPSET>
int ClocklessController_LCD_ESP32S3<DATA_PIN, RGB_ORDER, CHIPSET>::sLaneCount = 0;

FASTLED_NAMESPACE_END