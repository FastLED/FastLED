/// @file test_rmt_memory_manager.cpp
/// @brief Unit tests for RMT memory manager
///
/// These tests verify the memory accounting logic for ESP32 RMT5 driver
/// using the test constructor to mock different platform configurations.

#include "test.h"

#ifdef ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_RMT5

#include "platforms/esp/32/drivers/rmt/rmt_5/rmt_memory_manager.h"

using namespace fl;

// ============================================================================
// Test Suite 1: Basic Allocation Tests
// ============================================================================

FL_TEST_CASE("RMT Memory Manager: DMA TX channel memory consumption") {
    RmtMemoryManager mgr(192, 192, false);  // ESP32-S3 mock

    // Note: DMA memory behavior is platform-dependent.
    // ESP32-S3: DMA channels consume 1 memory block (48 words) for descriptor
    // Other platforms (if DMA supported): May bypass on-chip memory


    FL_SUBCASE("DMA TX allocation returns 0 words on non-S3 platforms") {
        auto result = mgr.allocateTx(0, true, false);  // use_dma=true

        FL_CHECK(result.ok());
        FL_CHECK_EQ(result.value(), 0);  // DMA uses DRAM, not on-chip memory
    }

    FL_SUBCASE("Memory accounting unchanged after DMA allocation on non-S3 platforms") {
        auto result = mgr.allocateTx(0, true, false);
        FL_CHECK(result.ok());

        // Verify memory pool unchanged
        FL_CHECK_EQ(mgr.getAllocatedTxWords(), 0);
        FL_CHECK_EQ(mgr.availableTxWords(), 192);
        FL_CHECK_EQ(mgr.getAllocationCount(), 1);
    }

}

FL_TEST_CASE("RMT Memory Manager: Non-DMA TX channel consumes on-chip memory") {
    RmtMemoryManager mgr(192, 192, false);  // ESP32-S3 mock

    FL_SUBCASE("Non-DMA TX allocation with network OFF (2× buffering)") {
        auto result = mgr.allocateTx(0, false, false);  // use_dma=false, network=false

        FL_CHECK(result.ok());
        FL_CHECK_EQ(result.value(), 96);  // 2 × 48 words
    }

    FL_SUBCASE("Memory accounting after non-DMA allocation") {
        auto result = mgr.allocateTx(0, false, false);
        FL_CHECK(result.ok());

        // Verify memory consumed
        FL_CHECK_EQ(mgr.getAllocatedTxWords(), 96);
        FL_CHECK_EQ(mgr.availableTxWords(), 96);  // 192 - 96
        FL_CHECK_EQ(mgr.getAllocationCount(), 1);
    }

    FL_SUBCASE("Non-DMA TX allocation with network ON (3× buffering)") {
        auto result = mgr.allocateTx(0, false, true);  // use_dma=false, network=true

        FL_CHECK(result.ok());
        FL_CHECK_EQ(result.value(), 144);  // 3 × 48 words

        FL_CHECK_EQ(mgr.getAllocatedTxWords(), 144);
        FL_CHECK_EQ(mgr.availableTxWords(), 48);  // 192 - 144
    }
}

FL_TEST_CASE("RMT Memory Manager: DMA RX channel bypasses on-chip memory") {
    RmtMemoryManager mgr(192, 192, false);  // ESP32-S3 mock

    FL_SUBCASE("DMA RX allocation returns 0 words") {
        auto result = mgr.allocateRx(0, 1024, true);  // use_dma=true

        FL_CHECK(result.ok());
        FL_CHECK_EQ(result.value(), 0);  // DMA uses DRAM
    }

    FL_SUBCASE("Memory accounting unchanged after DMA RX allocation") {
        auto result = mgr.allocateRx(0, 1024, true);
        FL_CHECK(result.ok());

        FL_CHECK_EQ(mgr.getAllocatedRxWords(), 0);
        FL_CHECK_EQ(mgr.availableRxWords(), 192);
        FL_CHECK_EQ(mgr.getAllocationCount(), 1);
    }
}

FL_TEST_CASE("RMT Memory Manager: Non-DMA RX channel consumes on-chip memory") {
    RmtMemoryManager mgr(192, 192, false);  // ESP32-S3 mock

    FL_SUBCASE("Non-DMA RX allocation with specified symbols") {
        auto result = mgr.allocateRx(0, 64, false);  // 64 symbols = 64 words

        FL_CHECK(result.ok());
        FL_CHECK_EQ(result.value(), 64);

        FL_CHECK_EQ(mgr.getAllocatedRxWords(), 64);
        FL_CHECK_EQ(mgr.availableRxWords(), 128);  // 192 - 64
    }
}

// ============================================================================
// Test Suite 2: Multi-Channel Tests (ESP32-S3 Scenario)
// ============================================================================

FL_TEST_CASE("ESP32-S3: 3 channels (1 DMA + 2 non-DMA) network OFF") {
    RmtMemoryManager mgr(192, 192, false);  // ESP32-S3: 192 TX words

    // Note: On ESP32-S3, DMA channels consume 48 words for descriptor
    // Memory accounting is now: DMA=48 + non-DMA*2=96 = 144 words for 3 channels (not 192)
    FL_SUBCASE("Channel 0: DMA (0 words on non-S3)") {
        auto result0 = mgr.allocateTx(0, true, false);
        FL_CHECK(result0.ok());
        FL_CHECK_EQ(result0.value(), 0);
        FL_CHECK_EQ(mgr.getAllocatedTxWords(), 0);
        FL_CHECK_EQ(mgr.availableTxWords(), 192);
    }

    FL_SUBCASE("Channels 0-1: DMA + non-DMA (96 words total on non-S3)") {
        mgr.allocateTx(0, true, false);   // 0 words
        auto result1 = mgr.allocateTx(1, false, false);  // 96 words

        FL_CHECK(result1.ok());
        FL_CHECK_EQ(result1.value(), 96);
        FL_CHECK_EQ(mgr.getAllocatedTxWords(), 96);
        FL_CHECK_EQ(mgr.availableTxWords(), 96);
    }

    FL_SUBCASE("Channels 0-2: All 3 channels allocate successfully on non-S3") {
        auto result0 = mgr.allocateTx(0, true, false);   // 0 words
        auto result1 = mgr.allocateTx(1, false, false);  // 96 words
        auto result2 = mgr.allocateTx(2, false, false);  // 96 words

        FL_CHECK(result0.ok());
        FL_CHECK(result1.ok());
        FL_CHECK(result2.ok());

        FL_CHECK_EQ(mgr.getAllocatedTxWords(), 192);  // 0 + 96 + 96
        FL_CHECK_EQ(mgr.availableTxWords(), 0);
        FL_CHECK_EQ(mgr.getAllocationCount(), 3);
    }
}

FL_TEST_CASE("ESP32-S3: 3 channels network ON - memory exhaustion") {
    RmtMemoryManager mgr(192, 192, false);  // ESP32-S3: 192 TX words

    FL_SUBCASE("Channel 0: DMA (0 words on non-S3)") {
        auto result0 = mgr.allocateTx(0, true, true);  // network=true
        FL_CHECK(result0.ok());
        FL_CHECK_EQ(result0.value(), 0);
    }

    FL_SUBCASE("Channel 1: non-DMA (144 words with 3× buffering) on non-S3") {
        mgr.allocateTx(0, true, true);
        auto result1 = mgr.allocateTx(1, false, true);

        FL_CHECK(result1.ok());
        FL_CHECK_EQ(result1.value(), 144);  // 3 × 48 words
        FL_CHECK_EQ(mgr.getAllocatedTxWords(), 144);
        FL_CHECK_EQ(mgr.availableTxWords(), 48);  // Only 48 words left
    }

    FL_SUBCASE("Channel 2: SHOULD FAIL - only 48 words available, needs 144 on non-S3") {
        mgr.allocateTx(0, true, true);   // 0 words
        mgr.allocateTx(1, false, true);  // 144 words

        auto result2 = mgr.allocateTx(2, false, true);

        // Expected behavior: allocation should fail
        FL_CHECK(!result2.ok());
        FL_CHECK_EQ(result2.error(), RmtMemoryError::INSUFFICIENT_TX_MEMORY);
    }
}

FL_TEST_CASE("ESP32-S3: All non-DMA channels (network OFF)") {
    RmtMemoryManager mgr(192, 192, false);

    FL_SUBCASE("2 non-DMA channels succeed (192 words total)") {
        auto result0 = mgr.allocateTx(0, false, false);  // 96 words
        auto result1 = mgr.allocateTx(1, false, false);  // 96 words

        FL_CHECK(result0.ok());
        FL_CHECK(result1.ok());
        FL_CHECK_EQ(mgr.getAllocatedTxWords(), 192);
        FL_CHECK_EQ(mgr.availableTxWords(), 0);
    }

    FL_SUBCASE("3rd non-DMA channel fails (no memory left)") {
        mgr.allocateTx(0, false, false);
        mgr.allocateTx(1, false, false);

        auto result2 = mgr.allocateTx(2, false, false);
        FL_CHECK(!result2.ok());
        FL_CHECK_EQ(result2.error(), RmtMemoryError::INSUFFICIENT_TX_MEMORY);
    }
}

// ============================================================================
// Test Suite 3: External Reservation Tests
// ============================================================================

FL_TEST_CASE("ESP32-S3: External memory reservation reduces available pool") {
    RmtMemoryManager mgr(192, 192, false);

    FL_SUBCASE("Reserve 48 words for external RMT usage") {
        mgr.reserveExternalMemory(48, 0);

        FL_CHECK_EQ(mgr.availableTxWords(), 144);  // 192 - 48 reserved

        size_t reserved_tx = 0, reserved_rx = 0;
        mgr.getReservedMemory(reserved_tx, reserved_rx);
        FL_CHECK_EQ(reserved_tx, 48);
        FL_CHECK_EQ(reserved_rx, 0);
    }

    FL_SUBCASE("Allocation respects reservation") {
        mgr.reserveExternalMemory(48, 0);

        // Allocate 3× buffering channel (144 words) - should succeed
        auto result1 = mgr.allocateTx(0, false, true);
        FL_CHECK(result1.ok());
        FL_CHECK_EQ(result1.value(), 144);
        FL_CHECK_EQ(mgr.availableTxWords(), 0);  // 192 - 48 reserved - 144 allocated

        // Second 3× channel should fail (no memory left)
        auto result2 = mgr.allocateTx(1, false, true);
        FL_CHECK(!result2.ok());
        FL_CHECK_EQ(result2.error(), RmtMemoryError::INSUFFICIENT_TX_MEMORY);
    }
}

FL_TEST_CASE("ESP32-S3: Multiple external reservations accumulate") {
    RmtMemoryManager mgr(192, 192, false);

    mgr.reserveExternalMemory(32, 0);
    FL_CHECK_EQ(mgr.availableTxWords(), 160);

    mgr.reserveExternalMemory(16, 0);  // Additional reservation
    FL_CHECK_EQ(mgr.availableTxWords(), 144);  // 192 - 32 - 16

    size_t reserved_tx = 0, reserved_rx = 0;
    mgr.getReservedMemory(reserved_tx, reserved_rx);
    FL_CHECK_EQ(reserved_tx, 48);  // 32 + 16
}

// ============================================================================
// Test Suite 4: DMA Slot Exhaustion Test (ESP32-S3)
// ============================================================================

FL_TEST_CASE("ESP32-S3: Only 1 DMA channel available") {
    RmtMemoryManager mgr(192, 192, false);

    FL_SUBCASE("First DMA allocation succeeds") {
        FL_CHECK(mgr.isDMAAvailable());

        bool allocated = mgr.allocateDMA(0, true);
        FL_CHECK(allocated);
        FL_CHECK(!mgr.isDMAAvailable());
        FL_CHECK_EQ(mgr.getDMAChannelsInUse(), 1);
    }

    FL_SUBCASE("Second DMA allocation fails") {
        mgr.allocateDMA(0, true);

        bool allocated = mgr.allocateDMA(1, true);
        FL_CHECK(!allocated);  // DMA slot already taken
    }

    FL_SUBCASE("DMA becomes available after freeing") {
        mgr.allocateDMA(0, true);
        FL_CHECK(!mgr.isDMAAvailable());

        mgr.freeDMA(0, true);
        FL_CHECK(mgr.isDMAAvailable());
        FL_CHECK_EQ(mgr.getDMAChannelsInUse(), 0);

        // Can allocate DMA again
        bool allocated = mgr.allocateDMA(1, true);
        FL_CHECK(allocated);
    }
}

// ============================================================================
// Test Suite 5: Network Mode and Memory Block Calculation
// ============================================================================

FL_TEST_CASE("Memory block calculation based on network state") {
    FL_SUBCASE("Network OFF: 2× buffering") {
        size_t blocks = RmtMemoryManager::calculateMemoryBlocks(false);
        FL_CHECK_EQ(blocks, 2);
    }

    FL_SUBCASE("Network ON: 3× buffering (on platforms that support it)") {
        size_t blocks = RmtMemoryManager::calculateMemoryBlocks(true);
        // Should be 3 on ESP32-S3, but 2 on C3/C6/H2 (platform-dependent)
        FL_CHECK(blocks >= 2);
        FL_CHECK(blocks <= 3);
    }
}

// ============================================================================
// Test Suite 6: Platform Variant Tests
// ============================================================================

FL_TEST_CASE("ESP32-C3: Memory limits (96 TX, 96 RX)") {
    RmtMemoryManager mgr(96, 96, false);  // C3 has 96 TX, 96 RX

    FL_SUBCASE("Platform detection") {
        FL_CHECK_EQ(mgr.getTotalTxWords(), 96);
        FL_CHECK_EQ(mgr.getTotalRxWords(), 96);
        FL_CHECK(!mgr.isGlobalPool());  // Dedicated pools
    }

    FL_SUBCASE("Single 2× buffering channel succeeds (96 words)") {
        auto result = mgr.allocateTx(0, false, false);
        FL_CHECK(result.ok());
        FL_CHECK_EQ(result.value(), 96);
        FL_CHECK_EQ(mgr.availableTxWords(), 0);
    }

    FL_SUBCASE("3× buffering exceeds capacity (would need 144 words)") {
        // Network mode would want 3× buffering = 144 words, but only 96 available
        auto result = mgr.allocateTx(0, false, true);

        // On C3, should cap at 2× buffering or fail
        if (result.ok()) {
            FL_CHECK_EQ(result.value(), 96);  // Capped at 2×
        } else {
            FL_CHECK_EQ(result.error(), RmtMemoryError::INSUFFICIENT_TX_MEMORY);
        }
    }
}

FL_TEST_CASE("ESP32: Global pool (512 words)") {
    RmtMemoryManager mgr(512, 0, true);  // ESP32: 512 words global pool

    FL_SUBCASE("Platform detection") {
        FL_CHECK_EQ(mgr.getTotalTxWords(), 512);
        FL_CHECK_EQ(mgr.getTotalRxWords(), 0);  // Global pool, no separate RX
        FL_CHECK(mgr.isGlobalPool());
    }

    FL_SUBCASE("Multiple allocations from global pool") {
        auto tx1 = mgr.allocateTx(0, false, false);  // 96 words
        auto rx1 = mgr.allocateRx(1, 64, false);      // 64 words
        auto tx2 = mgr.allocateTx(2, false, false);  // 96 words

        FL_CHECK(tx1.ok());
        FL_CHECK(rx1.ok());
        FL_CHECK(tx2.ok());

        // All allocations consume from same pool
        size_t total = tx1.value() + rx1.value() + tx2.value();
        FL_CHECK_EQ(total, 256);  // 96 + 64 + 96
    }
}

// ============================================================================
// Test Suite 7: Edge Cases and Error Handling
// ============================================================================

FL_TEST_CASE("Edge case: Double allocation of same channel") {
    RmtMemoryManager mgr(192, 192, false);

    FL_SUBCASE("Allocating same TX channel twice fails") {
        auto result1 = mgr.allocateTx(0, false, false);
        FL_CHECK(result1.ok());

        auto result2 = mgr.allocateTx(0, false, false);
        FL_CHECK(!result2.ok());
        FL_CHECK_EQ(result2.error(), RmtMemoryError::CHANNEL_ALREADY_ALLOCATED);
    }

    FL_SUBCASE("TX and RX channels are separate namespaces") {
        auto tx = mgr.allocateTx(0, false, false);
        auto rx = mgr.allocateRx(0, 64, false);

        // Both should succeed (different pools)
        FL_CHECK(tx.ok());
        FL_CHECK(rx.ok());
    }
}

FL_TEST_CASE("Edge case: Free and reallocation") {
    RmtMemoryManager mgr(192, 192, false);

    FL_SUBCASE("Can reallocate after freeing") {
        mgr.allocateTx(0, false, false);
        FL_CHECK_EQ(mgr.getAllocatedTxWords(), 96);

        mgr.free(0, true);
        FL_CHECK_EQ(mgr.getAllocatedTxWords(), 0);
        FL_CHECK_EQ(mgr.availableTxWords(), 192);

        // Can allocate again
        auto result = mgr.allocateTx(0, false, false);
        FL_CHECK(result.ok());
    }
}

FL_TEST_CASE("Edge case: Reset clears all allocations") {
    RmtMemoryManager mgr(192, 192, false);

    mgr.allocateTx(0, false, false);
    mgr.allocateTx(1, false, false);
    mgr.allocateRx(0, 64, false);

    FL_CHECK_EQ(mgr.getAllocationCount(), 3);

    mgr.reset();

    FL_CHECK_EQ(mgr.getAllocationCount(), 0);
    FL_CHECK_EQ(mgr.getAllocatedTxWords(), 0);
    FL_CHECK_EQ(mgr.getAllocatedRxWords(), 0);
    FL_CHECK_EQ(mgr.availableTxWords(), 192);
    FL_CHECK_EQ(mgr.availableRxWords(), 192);
}

FL_TEST_CASE("Edge case: Zero-size allocation") {
    RmtMemoryManager mgr(192, 192, false);

    FL_SUBCASE("RX allocation with 0 symbols") {
        auto result = mgr.allocateRx(0, 0, false);

        // Should succeed with 0 words
        FL_CHECK(result.ok());
        FL_CHECK_EQ(result.value(), 0);
    }
}

FL_TEST_CASE("Edge case: Query methods") {
    RmtMemoryManager mgr(192, 192, false);

    FL_SUBCASE("canAllocateTx with DMA - platform dependent") {
        // On non-S3 platforms, DMA bypasses on-chip memory and always succeeds
        // On ESP32-S3, DMA requires 48 words for descriptor
        FL_CHECK(mgr.canAllocateTx(true, false));   // DMA, network OFF
        FL_CHECK(mgr.canAllocateTx(true, true));    // DMA, network ON
    }

    FL_SUBCASE("canAllocateTx respects available memory") {
        FL_CHECK(mgr.canAllocateTx(false, false));  // 96 words available

        mgr.allocateTx(0, false, false);  // Consume 96 words
        FL_CHECK(mgr.canAllocateTx(false, false));  // Still 96 words available

        mgr.allocateTx(1, false, false);  // Consume remaining 96 words
        FL_CHECK(!mgr.canAllocateTx(false, false));  // No memory left
    }

    FL_SUBCASE("canAllocateRx checks available RX pool") {
        FL_CHECK(mgr.canAllocateRx(64));
        FL_CHECK(mgr.canAllocateRx(192));
        FL_CHECK(!mgr.canAllocateRx(193));  // Exceeds capacity
    }
}

FL_TEST_CASE("Edge case: getAllocatedWords query") {
    RmtMemoryManager mgr(192, 192, false);

    mgr.allocateTx(0, false, false);  // 96 words
    mgr.allocateRx(1, 64, false);     // 64 words

    FL_CHECK_EQ(mgr.getAllocatedWords(0, true), 96);   // TX channel 0
    FL_CHECK_EQ(mgr.getAllocatedWords(1, false), 64);  // RX channel 1
    FL_CHECK_EQ(mgr.getAllocatedWords(2, true), 0);    // Not allocated
}

// ============================================================================
// Test Suite 8: Platform Detection Static Methods
// ============================================================================

FL_TEST_CASE("Platform detection static methods") {
    FL_SUBCASE("getPlatformTxWords returns valid limit") {
        size_t tx = RmtMemoryManager::getPlatformTxWords();
        FL_CHECK(tx > 0);
        // ESP32-S3: 192, ESP32-C3: 96, ESP32: 512
        FL_CHECK(tx >= 96);
    }

    FL_SUBCASE("getPlatformRxWords returns valid limit") {
        size_t rx = RmtMemoryManager::getPlatformRxWords();
        // Should be 0 for global pool or > 0 for dedicated pools
        FL_CHECK(rx >= 0);
    }

    FL_SUBCASE("isPlatformGlobalPool is consistent") {
        bool global = RmtMemoryManager::isPlatformGlobalPool();
        size_t rx = RmtMemoryManager::getPlatformRxWords();

        // If global pool, RX words should be 0
        if (global) {
            FL_CHECK_EQ(rx, 0);
        } else {
            FL_CHECK(rx > 0);
        }
    }
}

// ============================================================================
// Test Suite 6: Memory Block Strategy API (Phase 1A)
// ============================================================================

FL_TEST_CASE("RMT Memory Manager: setMemoryBlockStrategy and getMemoryBlockStrategy API") {
    RmtMemoryManager mgr(192, 192, false);  // ESP32-S3 mock

    FL_SUBCASE("Default strategy matches legacy defines") {
        size_t idle, network;
        mgr.getMemoryBlockStrategy(idle, network);

        FL_CHECK_EQ(idle, 2);      // FASTLED_RMT_MEM_BLOCKS default
        FL_CHECK_EQ(network, 3);   // FASTLED_RMT_MEM_BLOCKS_NETWORK_MODE default
    }

    FL_SUBCASE("Custom strategy within platform limits") {
        mgr.setMemoryBlockStrategy(1, 2);

        size_t idle, network;
        mgr.getMemoryBlockStrategy(idle, network);

        FL_CHECK_EQ(idle, 1);
        FL_CHECK_EQ(network, 2);
    }

    FL_SUBCASE("Zero blocks clamped to minimum 1") {
        mgr.setMemoryBlockStrategy(0, 0);

        size_t idle, network;
        mgr.getMemoryBlockStrategy(idle, network);

        FL_CHECK_EQ(idle, 1);      // Clamped from 0 to 1
        FL_CHECK_EQ(network, 1);   // Clamped from 0 to 1
    }

    FL_SUBCASE("Blocks exceeding platform limit are capped") {
        // ESP32-S3 has 192 TX words / 48 words_per_channel = 4 max blocks
        mgr.setMemoryBlockStrategy(10, 20);

        size_t idle, network;
        mgr.getMemoryBlockStrategy(idle, network);

        FL_CHECK_EQ(idle, 4);      // Capped from 10 to 4
        FL_CHECK_EQ(network, 4);   // Capped from 20 to 4
    }

    FL_SUBCASE("Strategy persists across multiple gets") {
        mgr.setMemoryBlockStrategy(1, 3);

        size_t idle1, network1;
        mgr.getMemoryBlockStrategy(idle1, network1);

        size_t idle2, network2;
        mgr.getMemoryBlockStrategy(idle2, network2);

        FL_CHECK_EQ(idle1, idle2);
        FL_CHECK_EQ(network1, network2);
        FL_CHECK_EQ(idle1, 1);
        FL_CHECK_EQ(network1, 3);
    }
}

FL_TEST_CASE("RMT Memory Manager: Platform-specific memory block limits") {
    FL_SUBCASE("ESP32-C3 mock: max 2 blocks (96 TX words / 48 = 2)") {
        RmtMemoryManager mgr(96, 96, false);

        mgr.setMemoryBlockStrategy(5, 5);  // Request 5 blocks

        size_t idle, network;
        mgr.getMemoryBlockStrategy(idle, network);

        FL_CHECK_EQ(idle, 2);      // Capped to platform limit
        FL_CHECK_EQ(network, 2);   // Capped to platform limit
    }

    FL_SUBCASE("ESP32 mock: max 8 blocks (512 TX words / 64 = 8)") {
        RmtMemoryManager mgr(512, 512, true);  // Global pool

        mgr.setMemoryBlockStrategy(10, 12);  // Request 10, 12 blocks

        size_t idle, network;
        mgr.getMemoryBlockStrategy(idle, network);

        FL_CHECK_EQ(idle, 8);      // Capped to platform limit
        FL_CHECK_EQ(network, 8);   // Capped to platform limit
    }
}

// ============================================================================
// Test Suite 9: calculateMemoryBlocks Integration with Custom Strategy
// ============================================================================

FL_TEST_CASE("RMT Memory Manager: calculateMemoryBlocks integration with custom strategy") {
    // Save default strategy before each subcase
    auto& mgr = RmtMemoryManager::instance();
    size_t saved_idle, saved_network;
    mgr.getMemoryBlockStrategy(saved_idle, saved_network);

    FL_SUBCASE("Default strategy behavior") {
        // Verify default strategy is used by calculateMemoryBlocks
        size_t idle_blocks = RmtMemoryManager::calculateMemoryBlocks(false);
        size_t network_blocks = RmtMemoryManager::calculateMemoryBlocks(true);

        // Default: 2× idle, 3× network (platform-dependent capping may occur)
        FL_CHECK_EQ(idle_blocks, 2);
        // Network blocks may be 2 or 3 depending on platform (C3/C6/H2 cap at 2)
        FL_CHECK(network_blocks >= 2);
        FL_CHECK(network_blocks <= 3);
    }

    FL_SUBCASE("Custom strategy (1× idle, 2× network)") {
        mgr.setMemoryBlockStrategy(1, 2);

        size_t idle_blocks = RmtMemoryManager::calculateMemoryBlocks(false);
        size_t network_blocks = RmtMemoryManager::calculateMemoryBlocks(true);

        FL_CHECK_EQ(idle_blocks, 1);
        FL_CHECK_EQ(network_blocks, 2);
    }

    FL_SUBCASE("Custom strategy (2× idle, 4× network) - platform limit enforcement") {
        mgr.setMemoryBlockStrategy(2, 4);

        size_t idle_blocks = RmtMemoryManager::calculateMemoryBlocks(false);
        size_t network_blocks = RmtMemoryManager::calculateMemoryBlocks(true);

        FL_CHECK_EQ(idle_blocks, 2);
        // Network blocks capped at platform max (C3/C6/H2: 2, S3: 4, ESP32: 8)
        FL_CHECK(network_blocks >= 2);
        FL_CHECK(network_blocks <= 4);
    }

    FL_SUBCASE("Switching between network active/idle uses correct strategy") {
        mgr.setMemoryBlockStrategy(1, 3);

        // Idle mode uses mIdleBlocks
        size_t idle1 = RmtMemoryManager::calculateMemoryBlocks(false);
        FL_CHECK_EQ(idle1, 1);

        // Network active uses mNetworkBlocks
        size_t network1 = RmtMemoryManager::calculateMemoryBlocks(true);
        FL_CHECK(network1 >= 2);  // May be capped on C3/C6/H2
        FL_CHECK(network1 <= 3);

        // Switch back to idle
        size_t idle2 = RmtMemoryManager::calculateMemoryBlocks(false);
        FL_CHECK_EQ(idle2, 1);

        // Results should be consistent across multiple calls
        FL_CHECK_EQ(idle1, idle2);
    }

    FL_SUBCASE("Strategy persists across calculateMemoryBlocks calls") {
        mgr.setMemoryBlockStrategy(2, 2);

        size_t idle1 = RmtMemoryManager::calculateMemoryBlocks(false);
        size_t network1 = RmtMemoryManager::calculateMemoryBlocks(true);
        size_t idle2 = RmtMemoryManager::calculateMemoryBlocks(false);
        size_t network2 = RmtMemoryManager::calculateMemoryBlocks(true);

        // Strategy should persist across multiple calls
        FL_CHECK_EQ(idle1, idle2);
        FL_CHECK_EQ(network1, network2);
        FL_CHECK_EQ(idle1, 2);
        FL_CHECK_EQ(network1, 2);
    }

    FL_SUBCASE("Zero-block clamping propagates through calculateMemoryBlocks") {
        mgr.setMemoryBlockStrategy(0, 0);  // Invalid, should clamp to 1

        size_t idle_blocks = RmtMemoryManager::calculateMemoryBlocks(false);
        size_t network_blocks = RmtMemoryManager::calculateMemoryBlocks(true);

        // Both should be clamped to minimum 1
        FL_CHECK_EQ(idle_blocks, 1);
        FL_CHECK_EQ(network_blocks, 1);
    }

    FL_SUBCASE("Platform-specific constraint enforcement in calculateMemoryBlocks") {
        // Request strategy that may exceed platform limits
        mgr.setMemoryBlockStrategy(10, 10);

        size_t idle_blocks = RmtMemoryManager::calculateMemoryBlocks(false);
        size_t network_blocks = RmtMemoryManager::calculateMemoryBlocks(true);

        // Platform limits (ESP32-C3: 2, ESP32-S3: 4, ESP32: 8)
        size_t max_blocks = SOC_RMT_TX_CANDIDATES_PER_GROUP;
        FL_CHECK(idle_blocks <= max_blocks);
        FL_CHECK(network_blocks <= max_blocks);

#if SOC_RMT_TX_CANDIDATES_PER_GROUP < 3
        // C3/C6/H2 platforms: hard cap at 2 blocks
        FL_CHECK_EQ(idle_blocks, 2);
        FL_CHECK_EQ(network_blocks, 2);
#else
        // S3/ESP32 platforms: use platform max
        FL_CHECK_EQ(idle_blocks, max_blocks);
        FL_CHECK_EQ(network_blocks, max_blocks);
#endif
    }

    // Restore default strategy after all subcases
    mgr.setMemoryBlockStrategy(saved_idle, saved_network);
}

#endif // FASTLED_RMT5
#endif // ESP32
