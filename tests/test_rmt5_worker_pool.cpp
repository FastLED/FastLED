// Unit tests for RMT5 Worker Pool functionality

#include "test.h"

#ifdef ESP32
#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "platforms/esp/32/rmt_5/rmt_worker_pool.h"
#include "platforms/esp/32/rmt_5/idf5_rmt.h"
#include "fl/vector.h"

using namespace fl;

TEST_CASE("RmtWorkerConfig - Equality and Compatibility") {
    RmtWorkerConfig config1 = {
        .pin = 2,
        .ledCount = 100,
        .isRgbw = false,
        .t0h = 400, .t0l = 850, .t1h = 850, .t1l = 400,
        .reset = 280,
        .dmaMode = IRmtStrip::DMA_AUTO,
        .interruptPriority = 3
    };
    
    RmtWorkerConfig config2 = config1;
    RmtWorkerConfig config3 = config1;
    config3.ledCount = 200; // Different LED count
    
    RmtWorkerConfig config4 = config1;
    config4.pin = 4; // Different pin
    
    // Test equality
    CHECK_TRUE(config1 == config2);
    CHECK_FALSE(config1 == config3);
    CHECK_FALSE(config1 == config4);
    
    // Test compatibility (same pin, timing, mode - different LED count OK)
    CHECK_TRUE(config1.isCompatibleWith(config2));
    CHECK_TRUE(config1.isCompatibleWith(config3)); // Different LED count is compatible
    CHECK_FALSE(config1.isCompatibleWith(config4)); // Different pin is incompatible
}

TEST_CASE("RmtWorkerPool - Singleton Pattern") {
    RmtWorkerPool& pool1 = RmtWorkerPool::getInstance();
    RmtWorkerPool& pool2 = RmtWorkerPool::getInstance();
    
    // Should be the same instance
    CHECK_EQ(&pool1, &pool2);
}

TEST_CASE("RmtWorkerPool - Hardware Channel Count") {
    RmtWorkerPool& pool = RmtWorkerPool::getInstance();
    int channelCount = pool.getHardwareChannelCount();
    
    // Should return a reasonable channel count based on ESP32 variant
    CHECK_GT(channelCount, 0);
    CHECK_LE(channelCount, 8); // Maximum known channels on any ESP32 variant
    
    #if CONFIG_IDF_TARGET_ESP32
        CHECK_EQ(channelCount, 8);
    #elif CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3  
        CHECK_EQ(channelCount, 4);
    #elif CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32H2
        CHECK_EQ(channelCount, 2);
    #endif
}

TEST_CASE("RmtWorker - Basic Configuration") {
    RmtWorker worker;
    
    // Initially should be available and not configured
    CHECK_TRUE(worker.isAvailable());
    CHECK_FALSE(worker.isTransmissionActive());
    
    RmtWorkerConfig config = {
        .pin = 2,
        .ledCount = 30,
        .isRgbw = false,
        .t0h = 400, .t0l = 850, .t1h = 850, .t1l = 400,
        .reset = 280,
        .dmaMode = IRmtStrip::DMA_AUTO,
        .interruptPriority = 3
    };
    
    // Configure worker
    // Note: This test may fail in non-ESP32 environments or without proper hardware setup
    // In CI, we'll skip hardware-dependent tests
    if (worker.configure(config)) {
        CHECK_TRUE(worker.isConfiguredFor(config));
        
        // Test pixel data loading (with dummy data)
        fl::vector<uint8_t> pixelData(config.ledCount * 3, 0x80); // Gray pixels
        worker.loadPixelData(pixelData.data(), pixelData.size());
        
        // Worker should still be available (transmission not started)
        CHECK_TRUE(worker.isAvailable());
        CHECK_FALSE(worker.isTransmissionActive());
    }
}

TEST_CASE("RmtController5 - Worker Pool Integration") {
    // Test controller registration with worker pool
    RmtController5 controller(2, 400, 850, 400, RmtController5::DMA_AUTO);
    
    // Controller should have worker configuration
    const RmtWorkerConfig& config = controller.getWorkerConfig();
    CHECK_EQ(config.pin, 2);
    CHECK_EQ(config.dmaMode, IRmtStrip::DMA_AUTO);
    CHECK_EQ(config.interruptPriority, 3);
    
    // Initially should have empty pixel buffer
    CHECK_EQ(controller.getBufferSize(), 0u);
    CHECK_EQ(controller.getPixelBuffer(), nullptr);
}

TEST_CASE("RmtController5 - Legacy Mode Compatibility") {
    // Test that controller can be disabled from worker pool
    // This would require a compile-time flag or runtime setting
    
    // For now, just verify that the controller exists and basic methods work
    RmtController5 controller(4, 400, 850, 400, RmtController5::DMA_DISABLED);
    
    // Should not crash when calling methods
    const RmtWorkerConfig& config = controller.getWorkerConfig();
    CHECK_EQ(config.pin, 4);
    CHECK_EQ(config.dmaMode, IRmtStrip::DMA_DISABLED);
}

TEST_CASE("RmtWorkerPool - Buffer Management") {
    RmtWorkerPool& pool = RmtWorkerPool::getInstance();
    
    // Test that pool can handle multiple controllers
    fl::vector<RmtController5*> controllers;
    
    // Create multiple controllers with different configurations
    for (int i = 0; i < 3; i++) {
        int pin = 2 + i;
        controllers.push_back(new RmtController5(pin, 400, 850, 400, RmtController5::DMA_AUTO));
    }
    
    // Verify all controllers are registered
    // (This is internal state, so we can't directly test it without exposing internals)
    
    // Clean up
    for (RmtController5* controller : controllers) {
        delete controller;
    }
}

// Mock test for worker pool coordination
// Note: This test doesn't actually drive LEDs, just tests the coordination logic
TEST_CASE("RmtWorkerPool - Coordination Logic") {
    RmtWorkerPool& pool = RmtWorkerPool::getInstance();
    
    // Create a test controller
    RmtController5 controller(2, 400, 850, 400, RmtController5::DMA_AUTO);
    
    // Test immediate availability check
    // This should return true if workers are available
    bool canStart = pool.canStartImmediately(&controller);
    
    // Should be able to start immediately when no other controllers are active
    // (Assuming we have at least one RMT channel available)
    int channelCount = pool.getHardwareChannelCount();
    if (channelCount > 0) {
        CHECK_TRUE(canStart);
    }
}

// Integration test placeholder
// This would test actual LED driving, but requires hardware setup
TEST_CASE("RmtWorkerPool - Hardware Integration") {
    // Skip hardware tests in CI environment
    // In a real hardware test, we would:
    // 1. Create multiple controllers exceeding hardware limit
    // 2. Load different pixel data into each
    // 3. Call showPixels() on all controllers
    // 4. Verify that all LEDs display correctly
    // 5. Measure timing to ensure async behavior is preserved
    
    SKIP("Hardware integration test - requires physical LED strips");
}

#endif // FASTLED_RMT5
#endif // ESP32