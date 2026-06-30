// IWYU pragma: private

/// @file rmt4_peripheral_mock.cpp.hpp
/// @brief Concrete `Rmt4PeripheralMock` implementation (#3462).

// Host-only. Mirrors the rmt5 mock's gate.
#include "platforms/is_platform.h"
#if defined(FASTLED_STUB_IMPL) ||                                              \
    (!defined(ARDUINO) &&                                                      \
     (defined(FL_IS_LINUX) || defined(FL_IS_APPLE) || defined(FL_IS_WIN)))

#include "platforms/shared/mock/esp/32/drivers/rmt4_peripheral_mock.h"

#include "fl/log/log.h"
#include "fl/stl/bit_cast.h"
#include "fl/stl/cstdlib.h"
#include "fl/stl/flat_map.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/singleton.h"

namespace fl {
namespace detail {

//=============================================================================
// Concrete impl (private to this TU)
//=============================================================================

class Rmt4PeripheralMockImpl : public Rmt4PeripheralMock {
  public:
    Rmt4PeripheralMockImpl() FL_NO_EXCEPT
        : mChannels(),
          mNextIsrCookie(1),
          mIsrHandler(nullptr),
          mIsrArg(nullptr),
          mIsrCookie(nullptr),
          mFailConfigureChannel(false),
          mFailInstallDriver(false),
          mFailSetTxThresholdIntrEnable(false),
          mFailSetTxIntrEnable(false),
          mFailSetGpio(false),
          mFailInstallIsr(false),
          mIsrInvocations(0),
          mLastSimKind(SimulatedKind::NONE),
          mLastSimChannel(-1) {}

    ~Rmt4PeripheralMockImpl() override = default;

    //=== IRMT4Peripheral =====================================================

    bool configureChannel(const Rmt4ChannelConfig &cfg) FL_NO_EXCEPT override {
        if (mFailConfigureChannel) {
            return false;
        }
        Rmt4ChannelMockState &state = mChannels[cfg.mChannel];
        state.last_config = cfg;
        state.any_call_recorded = true;
        return true;
    }

    bool installDriver(int channel) FL_NO_EXCEPT override {
        if (mFailInstallDriver) {
            return false;
        }
        Rmt4ChannelMockState &state = mChannels[channel];
        state.driver_installed = true;
        state.any_call_recorded = true;
        return true;
    }

    bool uninstallDriver(int channel) FL_NO_EXCEPT override {
        Rmt4ChannelMockState &state = mChannels[channel];
        state.driver_installed = false;
        state.any_call_recorded = true;
        return true;
    }

    bool setTxThresholdIntrEnable(int channel, bool enable,
                                  u16 threshold) FL_NO_EXCEPT override {
        if (mFailSetTxThresholdIntrEnable) {
            return false;
        }
        Rmt4ChannelMockState &state = mChannels[channel];
        state.threshold_intr_enabled = enable;
        state.last_threshold = threshold;
        state.any_call_recorded = true;
        return true;
    }

    bool setTxIntrEnable(int channel, bool enable) FL_NO_EXCEPT override {
        if (mFailSetTxIntrEnable) {
            return false;
        }
        Rmt4ChannelMockState &state = mChannels[channel];
        state.tx_intr_enabled = enable;
        state.any_call_recorded = true;
        return true;
    }

    bool setGpio(int channel, int gpio_pin, bool invert) FL_NO_EXCEPT override {
        if (mFailSetGpio) {
            return false;
        }
        Rmt4ChannelMockState &state = mChannels[channel];
        state.last_gpio_pin = gpio_pin;
        state.last_gpio_invert = invert;
        state.any_call_recorded = true;
        return true;
    }

    bool installIsr(Rmt4IsrHandler handler, void *arg,
                    void **out_handle) FL_NO_EXCEPT override {
        if (out_handle == nullptr) {
            return false;
        }
        if (mFailInstallIsr) {
            *out_handle = nullptr;
            return false;
        }
        mIsrHandler = handler;
        mIsrArg = arg;
        // Hand back a unique non-null cookie. We just monotonically
        // bump a counter and cast it to void* — the test only needs
        // identity comparison and non-null check.
        intptr_t cookie_id = mNextIsrCookie++;
        mIsrCookie = fl::bit_cast<void *>(cookie_id);
        *out_handle = mIsrCookie;
        return true;
    }

    void freeIsr(void *handle) FL_NO_EXCEPT override {
        // Only clear if the caller passed back the cookie we issued.
        // Spurious nullptr calls are a no-op (matches the real impl).
        if (handle == nullptr) {
            return;
        }
        if (handle == mIsrCookie) {
            mIsrHandler = nullptr;
            mIsrArg = nullptr;
            mIsrCookie = nullptr;
        }
    }

    //=== Mock-only simulation ================================================

    void simulateThresholdInterrupt(int channel) FL_NO_EXCEPT override {
        mLastSimKind = SimulatedKind::THRESHOLD;
        mLastSimChannel = channel;
        invokeIsr();
    }

    void simulateTxDoneInterrupt(int channel) FL_NO_EXCEPT override {
        mLastSimKind = SimulatedKind::TX_DONE;
        mLastSimChannel = channel;
        invokeIsr();
    }

    SimulatedKind lastSimulatedKind() const FL_NO_EXCEPT override {
        return mLastSimKind;
    }

    int lastSimulatedChannel() const FL_NO_EXCEPT override {
        return mLastSimChannel;
    }

    u32 isrInvocationCount() const FL_NO_EXCEPT override {
        return mIsrInvocations;
    }

    //=== Error injection =====================================================

    void setConfigureChannelFailure(bool fail) FL_NO_EXCEPT override {
        mFailConfigureChannel = fail;
    }
    void setInstallDriverFailure(bool fail) FL_NO_EXCEPT override {
        mFailInstallDriver = fail;
    }
    void setTxThresholdIntrEnableFailure(bool fail) FL_NO_EXCEPT override {
        mFailSetTxThresholdIntrEnable = fail;
    }
    void setTxIntrEnableFailure(bool fail) FL_NO_EXCEPT override {
        mFailSetTxIntrEnable = fail;
    }
    void setGpioFailure(bool fail) FL_NO_EXCEPT override {
        mFailSetGpio = fail;
    }
    void setInstallIsrFailure(bool fail) FL_NO_EXCEPT override {
        mFailInstallIsr = fail;
    }

    //=== State inspection ====================================================

    const Rmt4ChannelMockState &
    getChannelState(int channel) const FL_NO_EXCEPT override {
        auto it = mChannels.find(channel);
        if (it == mChannels.end()) {
            return mEmptyState;
        }
        return it->second;
    }

    bool isIsrRegistered() const FL_NO_EXCEPT override {
        return mIsrCookie != nullptr;
    }

    Rmt4IsrHandler getRegisteredIsrHandler() const FL_NO_EXCEPT override {
        return mIsrHandler;
    }

    void *getRegisteredIsrArg() const FL_NO_EXCEPT override { return mIsrArg; }

    //=== Reset ==============================================================

    void reset() FL_NO_EXCEPT override {
        mChannels.clear();
        mIsrHandler = nullptr;
        mIsrArg = nullptr;
        mIsrCookie = nullptr;
        // mNextIsrCookie intentionally NOT reset — keeps cookies
        // monotonically unique across resets within a process, which
        // makes cookie-aliasing bugs in callers easier to spot.
        mFailConfigureChannel = false;
        mFailInstallDriver = false;
        mFailSetTxThresholdIntrEnable = false;
        mFailSetTxIntrEnable = false;
        mFailSetGpio = false;
        mFailInstallIsr = false;
        mIsrInvocations = 0;
        mLastSimKind = SimulatedKind::NONE;
        mLastSimChannel = -1;
    }

  private:
    void invokeIsr() FL_NO_EXCEPT {
        if (mIsrHandler != nullptr) {
            ++mIsrInvocations;
            (*mIsrHandler)(mIsrArg);
        }
    }

    fl::flat_map<int, Rmt4ChannelMockState> mChannels;
    Rmt4ChannelMockState mEmptyState; // returned when channel unknown

    intptr_t mNextIsrCookie;
    Rmt4IsrHandler mIsrHandler;
    void *mIsrArg;
    void *mIsrCookie;

    bool mFailConfigureChannel;
    bool mFailInstallDriver;
    bool mFailSetTxThresholdIntrEnable;
    bool mFailSetTxIntrEnable;
    bool mFailSetGpio;
    bool mFailInstallIsr;

    u32 mIsrInvocations;
    SimulatedKind mLastSimKind;
    int mLastSimChannel;
};

//=============================================================================
// Singleton accessor
//=============================================================================

Rmt4PeripheralMock &Rmt4PeripheralMock::instance() FL_NO_EXCEPT {
    return Singleton<Rmt4PeripheralMockImpl>::instance();
}

} // namespace detail
} // namespace fl

#endif // host build
