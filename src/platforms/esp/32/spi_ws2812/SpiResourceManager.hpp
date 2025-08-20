#pragma once

#ifdef ESP32

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "third_party/espressif/led_strip/src/led_strip.h"
#include "fl/namespace.h"

FASTLED_NAMESPACE_BEGIN

class SpiResourceManager {
private:
    struct SpiHostInfo {
        spi_host_device_t spi_host;
        bool used_by_fastled;
        bool bus_initialized_by_us;
    };
    
#if SOC_SPI_PERIPH_NUM > 2
    static constexpr int MAX_SPI_HOSTS = 3;
#else
    static constexpr int MAX_SPI_HOSTS = 2;
#endif
    SpiHostInfo mSpiHosts[MAX_SPI_HOSTS];
    SemaphoreHandle_t mMutex;
    static const char* TAG;
    
    SpiResourceManager() {
        mMutex = xSemaphoreCreateMutex();
        // Initialize SPI host info array in order of preference
        mSpiHosts[0] = {SPI2_HOST, false, false};
#if SOC_SPI_PERIPH_NUM > 2
        mSpiHosts[1] = {SPI3_HOST, false, false};
        mSpiHosts[2] = {SPI1_HOST, false, false};
#else
        mSpiHosts[1] = {SPI1_HOST, false, false};
#endif
    }
    
    ~SpiResourceManager() {
        if (mMutex) {
            vSemaphoreDelete(mMutex);
        }
    }
    
    // Non-copyable
    SpiResourceManager(const SpiResourceManager&) = delete;
    SpiResourceManager& operator=(const SpiResourceManager&) = delete;
    
public:
    static SpiResourceManager& getInstance() {
        static SpiResourceManager instance;  // Thread-safe in C++11+
        return instance;
    }
    
    bool isSpiHostAvailable(spi_host_device_t spi_host) {
        if (xSemaphoreTake(mMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGW(TAG, "Failed to acquire mutex for SPI host availability check");
            return false;
        }
        
        // Check if already used by FastLED
        for (int i = 0; i < MAX_SPI_HOSTS; i++) {
            if (mSpiHosts[i].spi_host == spi_host && mSpiHosts[i].used_by_fastled) {
                ESP_LOGD(TAG, "SPI host %d already in use by FastLED", spi_host);
                xSemaphoreGive(mMutex);
                return false;
            }
        }
        
        // In Arduino ESP32, we'll assume the host is available if not used by FastLED
        // The actual availability will be tested when trying to create the LED strip
        ESP_LOGD(TAG, "SPI host %d availability check: available (FastLED tracking)", spi_host);
        
        xSemaphoreGive(mMutex);
        return true;
    }
    
    esp_err_t getNextAvailableSpiHost(int pin, uint32_t led_count, bool with_dma, 
                                     spi_host_device_t* out_spi_host, led_strip_handle_t* out_strip) {
        if (!out_spi_host || !out_strip) {
            return ESP_ERR_INVALID_ARG;
        }
        
        if (xSemaphoreTake(mMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to acquire mutex for SPI host allocation");
            return ESP_ERR_TIMEOUT;
        }
        
        esp_err_t result = ESP_ERR_NOT_FOUND;
        
        for (int i = 0; i < MAX_SPI_HOSTS; i++) {
            spi_host_device_t spi_host = mSpiHosts[i].spi_host;
            
            // Skip if already used by FastLED
            if (mSpiHosts[i].used_by_fastled) {
                ESP_LOGD(TAG, "SPI host %d already in use by FastLED, skipping", spi_host);
                continue;
            }
            
            // In Arduino ESP32, we'll test availability by trying to create the LED strip directly
            
            // Try to create LED strip with this SPI host
            ESP_LOGD(TAG, "Attempting to use SPI host %d for LED strip", spi_host);
            
            led_strip_config_t strip_config = {
                .strip_gpio_num = pin,
                .max_leds = led_count,
                .led_model = LED_MODEL_WS2812,
                .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_RGB,
                .flags = {
                    .invert_out = false,
                },
                .timings = {},
            };
            
            led_strip_spi_config_t spi_config = {
                .clk_src = SPI_CLK_SRC_DEFAULT,
                .spi_bus = spi_host,
                .flags = {
                    .with_dma = with_dma,
                }
            };
            
            led_strip_handle_t led_strip;
            esp_err_t strip_result = led_strip_new_spi_device(&strip_config, &spi_config, &led_strip);
            
            if (strip_result == ESP_OK) {
                // Success! Mark as used and return
                mSpiHosts[i].used_by_fastled = true;
                mSpiHosts[i].bus_initialized_by_us = true;
                *out_spi_host = spi_host;
                *out_strip = led_strip;
                result = ESP_OK;
                ESP_LOGI(TAG, "Successfully allocated SPI host %d for LED strip", spi_host);
                break;
            } else {
                ESP_LOGD(TAG, "Failed to create LED strip on SPI host %d: %s", 
                         spi_host, esp_err_to_name(strip_result));
            }
        }
        
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "No available SPI hosts found for LED strip allocation");
        }
        
        xSemaphoreGive(mMutex);
        return result;
    }
    
    void releaseSpiHost(spi_host_device_t spi_host) {
        if (xSemaphoreTake(mMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGW(TAG, "Failed to acquire mutex for SPI host release");
            return;
        }
        
        for (int i = 0; i < MAX_SPI_HOSTS; i++) {
            if (mSpiHosts[i].spi_host == spi_host) {
                if (mSpiHosts[i].used_by_fastled) {
                    mSpiHosts[i].used_by_fastled = false;
                    mSpiHosts[i].bus_initialized_by_us = false;
                    ESP_LOGD(TAG, "Released SPI host %d", spi_host);
                } else {
                    ESP_LOGW(TAG, "Attempted to release SPI host %d that was not marked as used", spi_host);
                }
                break;
            }
        }
        
        xSemaphoreGive(mMutex);
    }
};

// Static member definition
const char* SpiResourceManager::TAG = "SpiResourceManager";

FASTLED_NAMESPACE_END

#endif // ESP32