/// @file rp_resource_ledger.cpp
/// @brief Host tests for the RP2040/RP2350 resource ownership contract.

#include "platforms/arm/rp/rpcommon/rp_resource_ledger.h"
#include "fl/stl/atomic.h"
#include "fl/stl/thread.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

FL_TEST_CASE("RP resource ledger rejects duplicate and out-of-range claims") {
    RpResourceLedger ledger(2, 4, 12, 30);

    FL_CHECK(ledger.claimPioStateMachine(0, 0));
    FL_CHECK_FALSE(ledger.claimPioStateMachine(0, 0));
    FL_CHECK(ledger.isPioStateMachineClaimed(0, 0));
    FL_CHECK_FALSE(ledger.claimPioStateMachine(2, 0));
    FL_CHECK_FALSE(ledger.claimPioStateMachine(0, 4));

    FL_CHECK(ledger.claimDmaChannel(11));
    FL_CHECK_FALSE(ledger.claimDmaChannel(12));
    FL_CHECK(ledger.claimUart(0));
    FL_CHECK_FALSE(ledger.claimUart(0));
    FL_CHECK_FALSE(ledger.claimUart(2));
    FL_CHECK(ledger.claimPin(29));
    FL_CHECK_FALSE(ledger.claimPin(30));
}

FL_TEST_CASE("RP resource ledger cleanup restores partial acquisitions") {
    RpResourceLedger ledger(2, 4, 2, 30);

    FL_REQUIRE(ledger.claimPioStateMachine(1, 3));
    FL_REQUIRE(ledger.claimDmaChannel(1));
    FL_REQUIRE(ledger.claimUart(1));
    FL_REQUIRE(ledger.claimPin(4));

    FL_CHECK(ledger.releasePin(4));
    FL_CHECK(ledger.releaseDmaChannel(1));
    FL_CHECK(ledger.releaseUart(1));
    FL_CHECK(ledger.releasePioStateMachine(1, 3));
    FL_CHECK_FALSE(ledger.releasePioStateMachine(1, 3));

    FL_CHECK(ledger.claimPioStateMachine(1, 3));
    FL_CHECK(ledger.claimDmaChannel(1));
    FL_CHECK(ledger.claimUart(1));
    FL_CHECK(ledger.claimPin(4));
}

FL_TEST_CASE("RP resource ledger permits one dual-core winner per resource") {
    RpResourceLedger ledger(2, 4, 12, 30);
    atomic<int> winners(0);
    constexpr int kContenders = 8;
    thread contenders[kContenders] = {
        thread([&]() { if (ledger.claimPioStateMachine(0, 1)) { winners.fetch_add(1); } }),
        thread([&]() { if (ledger.claimPioStateMachine(0, 1)) { winners.fetch_add(1); } }),
        thread([&]() { if (ledger.claimPioStateMachine(0, 1)) { winners.fetch_add(1); } }),
        thread([&]() { if (ledger.claimPioStateMachine(0, 1)) { winners.fetch_add(1); } }),
        thread([&]() { if (ledger.claimPioStateMachine(0, 1)) { winners.fetch_add(1); } }),
        thread([&]() { if (ledger.claimPioStateMachine(0, 1)) { winners.fetch_add(1); } }),
        thread([&]() { if (ledger.claimPioStateMachine(0, 1)) { winners.fetch_add(1); } }),
        thread([&]() { if (ledger.claimPioStateMachine(0, 1)) { winners.fetch_add(1); } }),
    };

    for (int index = 0; index < kContenders; ++index) {
        contenders[index].join();
    }

    FL_CHECK_EQ(winners.load(), 1);
    FL_CHECK(ledger.isPioStateMachineClaimed(0, 1));
}

}  // FL_TEST_FILE
