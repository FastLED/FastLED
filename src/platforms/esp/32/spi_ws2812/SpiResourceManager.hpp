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
    // Just track the preferred order of SPI hosts to try
#if SOC_SPI_PERIPH_NUM > 2
    static constexpr int MAX_SPI_HOSTS = 3;
    static constexpr spi_host_device_t SPI_HOST_ORDER[MAX_SPI_HOSTS] = {SPI2_HOST, SPI3_HOST, SPI1_HOST};
#else
    static constexpr int MAX_SPI_HOSTS = 2;
    static constexpr spi_host_device_t SPI_HOST_ORDER[MAX_SPI_HOSTS] = {SPI2_HOST, SPI1_HOST};
#endif
    
    SemaphoreHandle_t mMutex;
    static const char* TAG;
    
    SpiResourceManager() {
        mMutex = xSemaphoreCreateMutex();
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
        
        // Test if SPI bus is available by attempting to initialize it
        // This will fail with ESP_ERR_INVALID_STATE if already in use
        spi_bus_config_t bus_config = {
            .mosi_io_num = -1,  // Will be set when actually used
            .miso_io_num = -1,
            .sclk_io_num = -1,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 0,
            .flags = 0,
            .intr_flags = 0
        };
        
        esp_err_t init_result = spi_bus_initialize(spi_host, &bus_config, SPI_DMA_DISABLED);
        bool available = false;
        
        if (init_result == ESP_OK) {
            // Bus was not in use, we successfully initialized it
            // Now free it immediately since we were just testing
            spi_bus_free(spi_host);
            available = true;
            ESP_LOGD(TAG, "SPI host %d is available", spi_host);
        } else if (init_result == ESP_ERR_INVALID_STATE) {
            // Bus is already in use
            ESP_LOGD(TAG, "SPI host %d already in use", spi_host);
            available = false;
        } else {
            // Some other error occurred
            ESP_LOGW(TAG, "SPI host %d test failed: %s", spi_host, esp_err_to_name(init_result));
            available = false;
        }
        
        xSemaphoreGive(mMutex);
        return available;
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
        
        // Try each SPI host in order of preference
        for (int i = 0; i < MAX_SPI_HOSTS; i++) {
            spi_host_device_t spi_host = SPI_HOST_ORDER[i];
            
            ESP_LOGD(TAG, "Testing SPI host %d for availability", spi_host);
            
            // Try to create LED strip with this SPI host
            // The led_strip_new_spi_device will handle SPI bus initialization internally
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
                // Success! Return the working SPI host and strip
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
        
        // No internal state to track - the ESP32 SPI driver handles resource management
        // This function is kept for API compatibility but doesn't need to do anything
        ESP_LOGD(TAG, "Released SPI host %d (handled by ESP32 SPI driver)", spi_host);
        
        xSemaphoreGive(mMutex);
    }
};

// Static member definition
const char* SpiResourceManager::TAG = "SpiResourceManager";

FASTLED_NAMESPACE_END

#endif // ESP32