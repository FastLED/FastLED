#pragma once

// IWYU pragma: private

/// @file rp_pio_dma_resource_manager.h
/// @brief Pico SDK adapter for FastLED's RP PIO, DMA, and pin ownership ledger.

#include "platforms/arm/rp/is_rp.h"

#if defined(FL_IS_RP2040) || defined(FL_IS_RP2350)

#include "platforms/arm/rp/rpcommon/rp_resource_ledger.h"
#include "fl/stl/singleton.h"

// IWYU pragma: begin_keep
#include "hardware/dma.h"
// IWYU pragma: end_keep
// IWYU pragma: begin_keep
#include "hardware/pio.h"
// IWYU pragma: end_keep
// IWYU pragma: begin_keep
#include "hardware/sync.h"
// IWYU pragma: end_keep

namespace fl {

/// @brief Coordinates SDK claims with the host-testable FastLED ownership ledger.
///
/// Pico SDK claim functions protect hardware resources shared with user code.
/// The ledger mirrors successful SDK claims under a hardware spinlock, so
/// cleanup and partial-initialization rollback are deterministic across both
/// RP cores.
class RpPioDmaResourceManager {
  public:
    static RpPioDmaResourceManager& instance() FL_NO_EXCEPT {
        return Singleton<RpPioDmaResourceManager>::instance();
    }

    bool claimPioStateMachine(PIO* pio_out, int* state_machine_out) FL_NO_EXCEPT {
        LockGuard lock(*this);
        if (pio_out == nullptr || state_machine_out == nullptr) {
            return false;
        }

        for (u8 pio_index = 0; pio_index < NUM_PIOS; ++pio_index) {
            PIO pio = pioForIndex(pio_index);
            int state_machine = pio_claim_unused_sm(pio, false);
            if (state_machine < 0) {
                continue;
            }
            if (mLedger.claimPioStateMachine(pio_index, static_cast<u8>(state_machine))) {
                *pio_out = pio;
                *state_machine_out = state_machine;
                return true;
            }
            pio_sm_unclaim(pio, static_cast<uint>(state_machine));
        }
        return false;
    }

    /// @brief Atomically claim a PIO state machine and install a program.
    ///
    /// PIO instruction memory is shared by all four state machines in a
    /// block. Keeping the can-add/add pair inside the resource-manager lock
    /// prevents two RP cores from reserving the same apparent free slot.
    bool claimPioStateMachineAndAddProgram(const pio_program* program,
                                           PIO* pio_out, int* state_machine_out,
                                           int* program_offset_out) FL_NO_EXCEPT {
        LockGuard lock(*this);
        if (program == nullptr || pio_out == nullptr || state_machine_out == nullptr ||
            program_offset_out == nullptr) {
            return false;
        }
        for (u8 pio_index = 0; pio_index < NUM_PIOS; ++pio_index) {
            PIO pio = pioForIndex(pio_index);
            const int state_machine = pio_claim_unused_sm(pio, false);
            if (state_machine < 0) continue;
            if (!mLedger.claimPioStateMachine(pio_index, static_cast<u8>(state_machine))) {
                pio_sm_unclaim(pio, static_cast<uint>(state_machine));
                continue;
            }
            if (pio_can_add_program(pio, program)) {
                const uint offset = pio_add_program(pio, program);
                *pio_out = pio;
                *state_machine_out = state_machine;
                *program_offset_out = static_cast<int>(offset);
                return true;
            }
            pio_sm_unclaim(pio, static_cast<uint>(state_machine));
            mLedger.releasePioStateMachine(pio_index, static_cast<u8>(state_machine));
        }
        return false;
    }

    bool addPioProgram(PIO pio, const pio_program* program,
                       int* program_offset_out) FL_NO_EXCEPT {
        LockGuard lock(*this);
        if (pio == nullptr || program == nullptr || program_offset_out == nullptr ||
            !pio_can_add_program(pio, program)) {
            return false;
        }
        *program_offset_out = static_cast<int>(pio_add_program(pio, program));
        return true;
    }

    void removePioProgram(PIO pio, const pio_program* program,
                          int program_offset) FL_NO_EXCEPT {
        LockGuard lock(*this);
        if (pio == nullptr || program == nullptr || program_offset < 0) return;
        pio_remove_program(pio, program, static_cast<uint>(program_offset));
    }

    bool claimPioRxCaptureBuffer(void* owner) FL_NO_EXCEPT {
        LockGuard lock(*this);
        if (owner == nullptr || mPioRxCaptureOwner != nullptr) return false;
        mPioRxCaptureOwner = owner;
        return true;
    }

    void releasePioRxCaptureBuffer(void* owner) FL_NO_EXCEPT {
        LockGuard lock(*this);
        if (mPioRxCaptureOwner == owner) mPioRxCaptureOwner = nullptr;
    }

    void releasePioStateMachine(PIO pio, int state_machine) FL_NO_EXCEPT {
        LockGuard lock(*this);
        if (pio == nullptr || state_machine < 0) {
            return;
        }
        const int pio_index = pioIndex(pio);
        if (pio_index < 0) {
            return;
        }
        pio_sm_unclaim(pio, static_cast<uint>(state_machine));
        mLedger.releasePioStateMachine(static_cast<u8>(pio_index),
                                       static_cast<u8>(state_machine));
    }

    bool claimDmaChannel(int* channel_out) FL_NO_EXCEPT {
        LockGuard lock(*this);
        if (channel_out == nullptr) {
            return false;
        }
        const int channel = dma_claim_unused_channel(false);
        if (channel < 0) {
            return false;
        }
        if (mLedger.claimDmaChannel(static_cast<u8>(channel))) {
            *channel_out = channel;
            return true;
        }
        dma_channel_unclaim(static_cast<uint>(channel));
        return false;
    }

    void releaseDmaChannel(int channel) FL_NO_EXCEPT {
        LockGuard lock(*this);
        if (channel < 0) {
            return;
        }
        dma_channel_unclaim(static_cast<uint>(channel));
        mLedger.releaseDmaChannel(static_cast<u8>(channel));
    }

    /// @brief Claim a hardware UART block and mirror it in the FastLED ledger.
    ///
    /// Pico SDK exposes no uart_claim_* API.  Serial ownership is therefore
    /// coordinated exclusively under this manager's spinlock; callers must
    /// release it before returning the UART to user code.
    bool claimUart(u8 uart) FL_NO_EXCEPT {
        LockGuard lock(*this);
        return mLedger.claimUart(uart);
    }

    void releaseUart(u8 uart) FL_NO_EXCEPT {
        LockGuard lock(*this);
        mLedger.releaseUart(uart);
    }

    /// @brief Atomically claim a PL022 SPI block, its TX/RX DMA lanes, and
    /// the canonical MOSI/MISO/SCK GPIO triple.
    bool claimSpiDmaAndPins(u8 spi, u8 mosi_pin, u8 miso_pin, u8 sck_pin,
                            int* tx_dma_out, int* rx_dma_out) FL_NO_EXCEPT {
        LockGuard lock(*this);
        if (tx_dma_out == nullptr || rx_dma_out == nullptr ||
            !mLedger.claimSpi(spi)) {
            return false;
        }
        const u8 pins[] = {mosi_pin, miso_pin, sck_pin};
        u8 claimed_pins = 0;
        while (claimed_pins < 3 && mLedger.claimPin(pins[claimed_pins])) {
            ++claimed_pins;
        }
        if (claimed_pins != 3) {
            while (claimed_pins > 0) mLedger.releasePin(pins[--claimed_pins]);
            mLedger.releaseSpi(spi);
            return false;
        }
        const int tx_dma = dma_claim_unused_channel(false);
        if (tx_dma < 0 || !mLedger.claimDmaChannel(static_cast<u8>(tx_dma))) {
            if (tx_dma >= 0) dma_channel_unclaim(static_cast<uint>(tx_dma));
            for (u8 pin : pins) mLedger.releasePin(pin);
            mLedger.releaseSpi(spi);
            return false;
        }
        const int rx_dma = dma_claim_unused_channel(false);
        if (rx_dma < 0 || !mLedger.claimDmaChannel(static_cast<u8>(rx_dma))) {
            if (rx_dma >= 0) dma_channel_unclaim(static_cast<uint>(rx_dma));
            dma_channel_unclaim(static_cast<uint>(tx_dma));
            mLedger.releaseDmaChannel(static_cast<u8>(tx_dma));
            for (u8 pin : pins) mLedger.releasePin(pin);
            mLedger.releaseSpi(spi);
            return false;
        }
        *tx_dma_out = tx_dma;
        *rx_dma_out = rx_dma;
        return true;
    }

    void releaseSpi(u8 spi) FL_NO_EXCEPT {
        LockGuard lock(*this);
        mLedger.releaseSpi(spi);
    }

    /// @brief Atomically acquire the complete UART TX resource set.
    ///
    /// A UART driver must not momentarily own only the UART block while a PIO
    /// lane claims its pin (or vice versa).  This single critical section is
    /// the UART equivalent of a transaction: UART ownership, GPIO ownership,
    /// and an SDK DMA claim either all succeed or are rolled back here.
    bool claimUartDmaAndPin(u8 uart, u8 pin, int* dma_channel_out) FL_NO_EXCEPT {
        LockGuard lock(*this);
        if (dma_channel_out == nullptr || !mLedger.claimUart(uart)) {
            return false;
        }
        if (!mLedger.claimPin(pin)) {
            mLedger.releaseUart(uart);
            return false;
        }
        const int dma_channel = dma_claim_unused_channel(false);
        if (dma_channel < 0 ||
            !mLedger.claimDmaChannel(static_cast<u8>(dma_channel))) {
            if (dma_channel >= 0) {
                dma_channel_unclaim(static_cast<uint>(dma_channel));
            }
            mLedger.releasePin(pin);
            mLedger.releaseUart(uart);
            return false;
        }
        *dma_channel_out = dma_channel;
        return true;
    }

    /// @brief Atomically claim one PIO state machine, DMA channel, and GPIO.
    bool claimPioDmaAndPin(PIO* pio_out, int* state_machine_out,
                           int* dma_channel_out, u8 pin, u8 pin_count,
                           u8 pio_index) FL_NO_EXCEPT {
        LockGuard lock(*this);
        if (pio_out == nullptr || state_machine_out == nullptr ||
            dma_channel_out == nullptr) {
            return false;
        }
        if (pio_index >= NUM_PIOS) return false;
        PIO claimed_pio = pioForIndex(pio_index);
        const int claimed_sm = pio_claim_unused_sm(claimed_pio, false);
        if (claimed_sm >= 0 &&
            !mLedger.claimPioStateMachine(pio_index, static_cast<u8>(claimed_sm))) {
            pio_sm_unclaim(claimed_pio, static_cast<uint>(claimed_sm));
            return false;
        }
        if (claimed_sm < 0) {
            return false;
        }
        u8 claimed_pins = 0;
        while (claimed_pins < pin_count && mLedger.claimPin(pin + claimed_pins)) {
            ++claimed_pins;
        }
        if (claimed_pins != pin_count) {
            while (claimed_pins > 0) mLedger.releasePin(pin + --claimed_pins);
            pio_sm_unclaim(claimed_pio, static_cast<uint>(claimed_sm));
            mLedger.releasePioStateMachine(static_cast<u8>(pioIndex(claimed_pio)),
                                           static_cast<u8>(claimed_sm));
            return false;
        }
        const int dma = dma_claim_unused_channel(false);
        if (dma < 0 || !mLedger.claimDmaChannel(static_cast<u8>(dma))) {
            if (dma >= 0) dma_channel_unclaim(static_cast<uint>(dma));
            for (u8 offset = 0; offset < pin_count; ++offset) mLedger.releasePin(pin + offset);
            pio_sm_unclaim(claimed_pio, static_cast<uint>(claimed_sm));
            mLedger.releasePioStateMachine(static_cast<u8>(pioIndex(claimed_pio)),
                                           static_cast<u8>(claimed_sm));
            return false;
        }
        *pio_out = claimed_pio;
        *state_machine_out = claimed_sm;
        *dma_channel_out = dma;
        return true;
    }

    bool claimPins(u8 first_pin, u8 count) FL_NO_EXCEPT {
        LockGuard lock(*this);
        for (u8 offset = 0; offset < count; ++offset) {
            if (!mLedger.claimPin(first_pin + offset)) {
                while (offset > 0) {
                    --offset;
                    mLedger.releasePin(first_pin + offset);
                }
                return false;
            }
        }
        return true;
    }

    void releasePins(u8 first_pin, u8 count) FL_NO_EXCEPT {
        LockGuard lock(*this);
        for (u8 offset = 0; offset < count; ++offset) {
            mLedger.releasePin(first_pin + offset);
        }
    }

    RpPioDmaResourceManager() FL_NO_EXCEPT
        : mLedger(NUM_PIOS, RpResourceLedger::kMaxStateMachinesPerPio,
                  NUM_DMA_CHANNELS, RpResourceLedger::kMaxPins)
        , mLock(spin_lock_instance(spin_lock_claim_unused(true)))
        , mPioRxCaptureOwner(nullptr) {}

  private:
    class LockGuard {
      public:
        explicit LockGuard(RpPioDmaResourceManager& manager) FL_NO_EXCEPT
            : mLock(manager.mLock)
            , mInterruptState(spin_lock_blocking(mLock)) {}

        ~LockGuard() {
            spin_unlock(mLock, mInterruptState);
        }

      private:
        spin_lock_t* mLock;
        u32 mInterruptState;
    };

    static PIO pioForIndex(u8 index) FL_NO_EXCEPT {
        if (index == 0) {
            return pio0;
        }
        if (index == 1) {
            return pio1;
        }
#if defined(FL_IS_RP2350)
        return pio2;
#else
        return nullptr;
#endif
    }

    static int pioIndex(PIO pio) FL_NO_EXCEPT {
        if (pio == pio0) {
            return 0;
        }
        if (pio == pio1) {
            return 1;
        }
#if defined(FL_IS_RP2350)
        if (pio == pio2) {
            return 2;
        }
#endif
        return -1;
    }

    RpResourceLedger mLedger;
    spin_lock_t* mLock;
    void* mPioRxCaptureOwner;
};

}  // namespace fl

#endif  // defined(FL_IS_RP2040) || defined(FL_IS_RP2350)
