#pragma once


// Configuration settings for the led_strip.
#define LED_STRIP_RMT_DEFAULT_RESOLUTION 10000000 // 10MHz resolution

#ifndef FASTLED_RMT_INTERRUPT_PRIORITY
#define FASTLED_RMT_INTERRUPT_PRIORITY 3  // maximum priority level for RMT interrupt
#endif

#ifndef LED_STRIP_RMT_DEFAULT_TRANS_QUEUE_SIZE
#define LED_STRIP_RMT_DEFAULT_TRANS_QUEUE_SIZE 4
#endif  // LED_STRIP_RMT_DEFAULT_TRANS_QUEUE_SIZE

// the memory size of each RMT channel, in words (4 bytes)
#ifndef LED_STRIP_RMT_DEFAULT_MEM_BLOCK_SYMBOLS
// TODO: This can be a runtime parameter that adjust to the number of strips.
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
#define LED_STRIP_RMT_DEFAULT_MEM_BLOCK_SYMBOLS 64
#else
#define LED_STRIP_RMT_DEFAULT_MEM_BLOCK_SYMBOLS 48
#endif
#endif  // LED_STRIP_RMT_DEFAULT_MEM_BLOCK_SYMBOLS

#ifndef FASTLED_RMT_WITH_DMA
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(ESP8266) || defined(CONFIG_IDF_TARGET_ESP32C3)
// This can be a runtime parameter that adjusts to the chip detection feature of esp-idf.
 #define FASTLED_RMT_WITH_DMA 0
#else
 #define FASTLED_RMT_WITH_DMA 1
#endif
#endif  // FASTLED_RMT_WITH_DMA

#ifndef FASTLED_RMT_MEMBLOCK_SYMBOLS
#if FASTLED_RMT_WITH_DMA
// This can be a runtime parameter that adjusts to the chip detection feature of esp-idf.
#define FASTLED_RMT_MEMBLOCK_SYMBOLS 1024
#else
#define FASTLED_RMT_MEMBLOCK_SYMBOLS LED_STRIP_RMT_DEFAULT_MEM_BLOCK_SYMBOLS  // Let the library decide
#endif  // FASTLED_RMT_WITH_DMA


#ifndef FASTLED_RMT_MAX_CHANNELS
#define FASTLED_RMT_MAX_CHANNELS -1  // Allow runtime discovery
#endif  // FASTLED_RMT_MAX_CHANNELS

#endif  // FASTLED_RMT_MEMBLOCK_SYMBOLS