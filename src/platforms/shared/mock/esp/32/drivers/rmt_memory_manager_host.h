#pragma once

// IWYU pragma: private
// ok no namespace fl: compile-profile declarations for the isolated native TU.

#include "platforms/is_platform.h"
#ifdef FASTLED_STUB_IMPL
// The dedicated allocator test compiles production code with the existing
// 64-word host mock capability and an eight-channel accounting profile.
#define FL_RMT_MEMORY_MANAGER_TEST 1
#define SOC_RMT_MEM_WORDS_PER_CHANNEL 64
#define SOC_RMT_TX_CANDIDATES_PER_GROUP 8
#define FASTLED_RMT_MEM_BLOCKS 2
#define FASTLED_RMT_MEM_BLOCKS_NETWORK_MODE 2
#define FASTLED_RMT5_DMA_SUPPORTED 0
#define FASTLED_RMT5_MAX_DMA_CHANNELS 0
#include "platforms/esp/32/drivers/rmt/rmt_5/rmt_memory_manager.h"
#endif
