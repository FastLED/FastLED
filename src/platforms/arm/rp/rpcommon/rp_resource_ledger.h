#pragma once

// IWYU pragma: private

/// @file rp_resource_ledger.h
/// @brief Lock-free ownership ledger shared by RP2040/RP2350 PIO drivers.
///
/// The Pico SDK owns the hardware claim bits. This ledger records FastLED's
/// matching ownership so a driver can make rollback and cleanup deterministic
/// without relying on a particular peripheral implementation. It deliberately
/// has no Pico SDK dependency, which keeps the resource contract testable on
/// the host.

#include "fl/stl/atomic.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {

class RpResourceLedger {
  public:
    static constexpr u8 kMaxPioBlocks = 3;
    static constexpr u8 kMaxStateMachinesPerPio = 4;
    static constexpr u8 kMaxDmaChannels = 32;
    static constexpr u8 kMaxPins = 64;

    RpResourceLedger(u8 pio_blocks = kMaxPioBlocks,
                     u8 state_machines_per_pio = kMaxStateMachinesPerPio,
                     u8 dma_channels = kMaxDmaChannels,
                     u8 pins = kMaxPins) FL_NO_EXCEPT
        : mPioBlocks(pio_blocks > kMaxPioBlocks ? kMaxPioBlocks : pio_blocks)
        , mStateMachinesPerPio(state_machines_per_pio > kMaxStateMachinesPerPio
                                   ? kMaxStateMachinesPerPio
                                   : state_machines_per_pio)
        , mDmaChannels(dma_channels > kMaxDmaChannels ? kMaxDmaChannels : dma_channels)
        , mPins(pins > kMaxPins ? kMaxPins : pins) {
        reset();
    }

    bool claimPioStateMachine(u8 pio, u8 state_machine) FL_NO_EXCEPT {
        if (pio >= mPioBlocks || state_machine >= mStateMachinesPerPio) {
            return false;
        }
        return claimBit(mPioStateMachines[pio], state_machine);
    }

    bool releasePioStateMachine(u8 pio, u8 state_machine) FL_NO_EXCEPT {
        if (pio >= mPioBlocks || state_machine >= mStateMachinesPerPio) {
            return false;
        }
        return releaseBit(mPioStateMachines[pio], state_machine);
    }

    bool isPioStateMachineClaimed(u8 pio, u8 state_machine) const FL_NO_EXCEPT {
        if (pio >= mPioBlocks || state_machine >= mStateMachinesPerPio) {
            return false;
        }
        return isBitClaimed(mPioStateMachines[pio], state_machine);
    }

    bool claimDmaChannel(u8 channel) FL_NO_EXCEPT {
        return channel < mDmaChannels && claimBit(mDmaChannelsClaimed, channel);
    }

    bool releaseDmaChannel(u8 channel) FL_NO_EXCEPT {
        return channel < mDmaChannels && releaseBit(mDmaChannelsClaimed, channel);
    }

    bool isDmaChannelClaimed(u8 channel) const FL_NO_EXCEPT {
        return channel < mDmaChannels && isBitClaimed(mDmaChannelsClaimed, channel);
    }

    bool claimPin(u8 pin) FL_NO_EXCEPT {
        return pin < mPins && claimBit(mPinsClaimed[pin / 32], pin % 32);
    }

    bool releasePin(u8 pin) FL_NO_EXCEPT {
        return pin < mPins && releaseBit(mPinsClaimed[pin / 32], pin % 32);
    }

    bool isPinClaimed(u8 pin) const FL_NO_EXCEPT {
        return pin < mPins && isBitClaimed(mPinsClaimed[pin / 32], pin % 32);
    }

    void reset() FL_NO_EXCEPT {
        for (u8 pio = 0; pio < kMaxPioBlocks; ++pio) {
            mPioStateMachines[pio].store(0, memory_order_release);
        }
        mDmaChannelsClaimed.store(0, memory_order_release);
        for (u8 word = 0; word < 2; ++word) {
            mPinsClaimed[word].store(0, memory_order_release);
        }
    }

  private:
    static bool claimBit(atomic<u32>& claims, u8 bit) FL_NO_EXCEPT {
        const u32 mask = 1u << bit;
        u32 expected = claims.load(memory_order_acquire);
        while ((expected & mask) == 0) {
            if (claims.compare_exchange_weak(expected, expected | mask,
                                             memory_order_acq_rel)) {
                return true;
            }
        }
        return false;
    }

    static bool releaseBit(atomic<u32>& claims, u8 bit) FL_NO_EXCEPT {
        const u32 mask = 1u << bit;
        u32 expected = claims.load(memory_order_acquire);
        while ((expected & mask) != 0) {
            if (claims.compare_exchange_weak(expected, expected & ~mask,
                                             memory_order_acq_rel)) {
                return true;
            }
        }
        return false;
    }

    static bool isBitClaimed(const atomic<u32>& claims, u8 bit) FL_NO_EXCEPT {
        return (claims.load(memory_order_acquire) & (1u << bit)) != 0;
    }

    u8 mPioBlocks;
    u8 mStateMachinesPerPio;
    u8 mDmaChannels;
    u8 mPins;
    atomic<u32> mPioStateMachines[kMaxPioBlocks];
    atomic<u32> mDmaChannelsClaimed;
    atomic<u32> mPinsClaimed[2];
};

}  // namespace fl
