/// @file rmt_memory_manager_4.cpp
/// @brief Host tests for `RmtMemoryManager4` (#3469).
///
/// The manager is IDF-agnostic (it's a pure accounting ledger with no
/// ESP-IDF dependencies) so it builds and runs on the host directly.
/// Tests use the test-only pool-size constructor to control the
/// assertions independent of the platform's real 512-word pool.

#include "platforms/esp/32/drivers/rmt/rmt_4/rmt_memory_manager_4.h"

#include "fl/stl/cstddef.h"
#include "test.h"

using namespace fl;

FL_TEST_CASE("RmtMemoryManager4 - pool starts empty") {
    RmtMemoryManager4 mgr(/*total_words=*/512);
    FL_CHECK(mgr.poolSize() == 512u);
    FL_CHECK(mgr.allocatedWords() == 0u);
    FL_CHECK(mgr.availableWords() == 512u);
    FL_CHECK_FALSE(mgr.hasActiveRxChannels());
}

FL_TEST_CASE("RmtMemoryManager4 - TX idle allocation reserves 128 words") {
    RmtMemoryManager4 mgr(512);
    size_t got = 0;
    FL_CHECK(
        mgr.tryAllocateTx(/*channel_id=*/0, /*network_active=*/false, got));
    FL_CHECK(got == 128u);
    FL_CHECK(mgr.allocatedWords() == 128u);
    FL_CHECK(mgr.availableWords() == 384u);
}

FL_TEST_CASE(
    "RmtMemoryManager4 - TX network-active allocation reserves 192 words") {
    RmtMemoryManager4 mgr(512);
    size_t got = 0;
    FL_CHECK(mgr.tryAllocateTx(0, /*network_active=*/true, got));
    FL_CHECK(got == 192u);
    FL_CHECK(mgr.allocatedWords() == 192u);
}

FL_TEST_CASE(
    "RmtMemoryManager4 - graceful demotion when 192 doesn't fit but 128 does") {
    // Pool of 150 words: 128 (idle) fits, 192 (network-active) does not.
    RmtMemoryManager4 mgr(150);
    size_t got = 0;
    FL_CHECK(mgr.tryAllocateTx(0, /*network_active=*/true, got));
    FL_CHECK(got == 128u); // Demoted from 192.
    FL_CHECK(mgr.allocatedWords() == 128u);
}

FL_TEST_CASE("RmtMemoryManager4 - refuses TX when pool exhausted") {
    RmtMemoryManager4 mgr(200);
    size_t got = 0;
    FL_CHECK(mgr.tryAllocateTx(0, false, got));       // 128 words fit.
    FL_CHECK_FALSE(mgr.tryAllocateTx(1, false, got)); // 128 more do not.
    FL_CHECK(mgr.allocatedWords() == 128u);
}

FL_TEST_CASE(
    "RmtMemoryManager4 - refuses duplicate TX allocation on same channel") {
    RmtMemoryManager4 mgr(512);
    size_t got = 0;
    FL_CHECK(mgr.tryAllocateTx(3, false, got));
    FL_CHECK_FALSE(mgr.tryAllocateTx(3, false, got));
    FL_CHECK(mgr.allocatedWords() == 128u);
}

FL_TEST_CASE("RmtMemoryManager4 - RX allocation records symbol count") {
    RmtMemoryManager4 mgr(512);
    size_t got = 0;
    FL_CHECK(mgr.tryAllocateRx(/*channel_id=*/128 + 34, /*symbols=*/64, got));
    FL_CHECK(got == 64u);
    FL_CHECK(mgr.allocatedWords() == 64u);
    FL_CHECK(mgr.hasActiveRxChannels());
}

FL_TEST_CASE("RmtMemoryManager4 - RX rejects zero-symbol requests") {
    RmtMemoryManager4 mgr(512);
    size_t got = 0;
    FL_CHECK_FALSE(mgr.tryAllocateRx(200, 0, got));
    FL_CHECK(mgr.allocatedWords() == 0u);
    FL_CHECK_FALSE(mgr.hasActiveRxChannels());
}

FL_TEST_CASE("RmtMemoryManager4 - free() releases the words back to the pool") {
    RmtMemoryManager4 mgr(512);
    size_t got = 0;
    mgr.tryAllocateTx(0, false, got);
    FL_CHECK(mgr.allocatedWords() == 128u);
    FL_CHECK(mgr.free(0, /*is_tx=*/true));
    FL_CHECK(mgr.allocatedWords() == 0u);
    FL_CHECK(mgr.availableWords() == 512u);
}

FL_TEST_CASE("RmtMemoryManager4 - free() with wrong is_tx returns false") {
    RmtMemoryManager4 mgr(512);
    size_t got = 0;
    mgr.tryAllocateTx(0, false, got);
    FL_CHECK_FALSE(mgr.free(0, /*is_tx=*/false)); // wrong direction
    FL_CHECK(mgr.allocatedWords() == 128u);       // unchanged
}

FL_TEST_CASE(
    "RmtMemoryManager4 - free() on unknown channel returns false silently") {
    RmtMemoryManager4 mgr(512);
    FL_CHECK_FALSE(mgr.free(0, true));
    FL_CHECK(mgr.allocatedWords() == 0u);
}

FL_TEST_CASE("RmtMemoryManager4 - free-then-realloc is the reconfigure idiom") {
    RmtMemoryManager4 mgr(512);
    size_t got = 0;
    mgr.tryAllocateTx(0, false, got);          // idle
    (void)mgr.free(0, true);                   // "reconfigure" — free first
    FL_CHECK(mgr.tryAllocateTx(0, true, got)); // network-active
    FL_CHECK(got == 192u);
    FL_CHECK(mgr.allocatedWords() == 192u);
}

FL_TEST_CASE("RmtMemoryManager4 - hasActiveRxChannels tracks RX lifetime") {
    RmtMemoryManager4 mgr(512);
    size_t got = 0;
    FL_CHECK_FALSE(mgr.hasActiveRxChannels());
    mgr.tryAllocateRx(128 + 34, 64, got);
    FL_CHECK(mgr.hasActiveRxChannels());
    mgr.free(128 + 34, /*is_tx=*/false);
    FL_CHECK_FALSE(mgr.hasActiveRxChannels());
}

FL_TEST_CASE("RmtMemoryManager4 - hasActiveRxChannels ignores TX allocations") {
    RmtMemoryManager4 mgr(512);
    size_t got = 0;
    mgr.tryAllocateTx(0, false, got);
    FL_CHECK_FALSE(mgr.hasActiveRxChannels());
}

FL_TEST_CASE(
    "RmtMemoryManager4 - calculateMemoryBlocks returns 2 when network idle") {
    RmtMemoryManager4 mgr(512);
    FL_CHECK(mgr.calculateMemoryBlocks(/*network_active=*/false) == 2);
}

FL_TEST_CASE(
    "RmtMemoryManager4 - calculateMemoryBlocks returns 3 when active + room") {
    RmtMemoryManager4 mgr(512);
    FL_CHECK(mgr.calculateMemoryBlocks(/*network_active=*/true) == 3);
}

FL_TEST_CASE("RmtMemoryManager4 - calculateMemoryBlocks falls back to 2 when "
             "the extra block doesn't fit") {
    // Fill the pool to within 63 words of the limit; the extra 64-word
    // block for triple-buffering can't fit, so the recommendation must
    // gracefully demote.
    RmtMemoryManager4 mgr(200); // starts idle
    size_t got = 0;
    mgr.tryAllocateTx(0, /*network_active=*/false, got);
    // 200 - 128 = 72 words free — one 64-word block still fits.
    FL_CHECK(mgr.calculateMemoryBlocks(/*network_active=*/true) == 3);

    mgr.tryAllocateRx(128 + 34, /*symbols=*/16, got);
    // 72 - 16 = 56 words free — the extra 64-word block no longer fits.
    FL_CHECK(mgr.calculateMemoryBlocks(/*network_active=*/true) == 2);
}

FL_TEST_CASE("RmtMemoryManager4 - reset() clears every allocation") {
    RmtMemoryManager4 mgr(512);
    size_t got = 0;
    mgr.tryAllocateTx(0, false, got);
    mgr.tryAllocateTx(1, true, got);
    mgr.tryAllocateRx(128 + 33, 64, got);
    FL_CHECK(mgr.allocatedWords() > 0u);
    FL_CHECK(mgr.hasActiveRxChannels());

    mgr.reset();

    FL_CHECK(mgr.allocatedWords() == 0u);
    FL_CHECK_FALSE(mgr.hasActiveRxChannels());
    FL_CHECK(mgr.availableWords() == 512u);
}

FL_TEST_CASE(
    "RmtMemoryManager4 - multiple TX channels coexist up to pool limit") {
    // 512-word pool at 128 words per idle TX channel = 4 concurrent TX.
    RmtMemoryManager4 mgr(512);
    size_t got = 0;
    FL_CHECK(mgr.tryAllocateTx(0, false, got));
    FL_CHECK(mgr.tryAllocateTx(1, false, got));
    FL_CHECK(mgr.tryAllocateTx(2, false, got));
    FL_CHECK(mgr.tryAllocateTx(3, false, got));
    FL_CHECK(mgr.allocatedWords() == 512u);
    FL_CHECK(mgr.availableWords() == 0u);
    FL_CHECK_FALSE(mgr.tryAllocateTx(4, false, got)); // fifth doesn't fit
}
