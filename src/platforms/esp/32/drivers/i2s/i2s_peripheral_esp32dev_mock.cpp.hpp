// IWYU pragma: private

/// @file i2s_peripheral_esp32dev_mock.cpp.hpp
/// @brief Concrete host mock for `II2sPeripheralEsp32Dev` (#3471).

#include "platforms/is_platform.h"

#if defined(FASTLED_STUB_IMPL) ||                                              \
    (!defined(ARDUINO) &&                                                      \
     (defined(FL_IS_LINUX) || defined(FL_IS_APPLE) || defined(FL_IS_WIN)))

#include "platforms/esp/32/drivers/i2s/i2s_peripheral_esp32dev_mock.h"

#include "fl/log/log.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/cstring.h"
#include "fl/stl/malloc.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/singleton.h"

namespace fl {

//=============================================================================
// Impl — private to this TU
//=============================================================================

class I2sPeripheralEsp32DevMockImpl : public I2sPeripheralEsp32DevMock {
  public:
    I2sPeripheralEsp32DevMockImpl() FL_NO_EXCEPT
        : mConfig(),
          mInitialized(false),
          mBusy(false),
          mTxHistory(),
          mTxSequence(0),
          mCallback(nullptr),
          mCallbackUserCtx(nullptr),
          mAllocations(),
          mAllocateCount(0),
          mCallbackInvocations(0),
          mFailInitialize(false),
          mFailAllocateBuffer(false),
          mFailTransmit(false),
          mFailRegisterCallback(false) {}

    ~I2sPeripheralEsp32DevMockImpl() override {
        // Free every outstanding heap allocation so a test that
        // forgets to call reset() doesn't leak into the next test.
        for (auto *buf : mAllocations) {
            if (buf) {
                fl::free(buf);
            }
        }
    }

    //=== II2sPeripheralEsp32Dev ==============================================

    bool
    initialize(const I2sEsp32DevPeripheralConfig &cfg) FL_NO_EXCEPT override {
        if (mFailInitialize) {
            return false;
        }
        mConfig = cfg;
        mInitialized = true;
        return true;
    }

    void deinitialize() FL_NO_EXCEPT override {
        mInitialized = false;
        mBusy = false;
    }

    bool isInitialized() const FL_NO_EXCEPT override { return mInitialized; }

    u8 *allocateBuffer(size_t size_bytes) FL_NO_EXCEPT override {
        if (mFailAllocateBuffer) {
            return nullptr;
        }
        u8 *buf = static_cast<u8 *>(fl::malloc(size_bytes));
        if (buf != nullptr) {
            mAllocations.push_back(buf);
            ++mAllocateCount;
        }
        return buf;
    }

    void freeBuffer(u8 *buffer) FL_NO_EXCEPT override {
        if (buffer == nullptr) {
            return;
        }
        for (size_t i = 0; i < mAllocations.size(); ++i) {
            if (mAllocations[i] == buffer) {
                fl::free(buffer);
                // Swap-remove is fine — order isn't observed.
                mAllocations[i] = mAllocations.back();
                mAllocations.pop_back();
                return;
            }
        }
        FL_WARN_F("I2sPeripheralEsp32DevMock: freeBuffer() on unknown ptr");
    }

    bool transmit(const u8 *buffer, size_t size_bytes) FL_NO_EXCEPT override {
        if (mFailTransmit) {
            return false;
        }
        if (!mInitialized) {
            return false;
        }
        if (mBusy) {
            FL_WARN_F(
                "I2sPeripheralEsp32DevMock: transmit() while already busy");
            return false;
        }
        // Capture a copy of the pixel bytes so tests can assert on
        // what the engine packed. Match rmt5 mock semantics — every
        // call gets its own record.
        I2sEsp32DevTransmitRecord rec;
        rec.buffer_copy.resize(size_bytes);
        if (buffer != nullptr && size_bytes > 0) {
            fl::memcpy(rec.buffer_copy.data(), buffer, size_bytes);
        }
        rec.size_bytes = size_bytes;
        rec.sequence = ++mTxSequence;
        mTxHistory.push_back(rec);
        mBusy = true;
        return true;
    }

    bool waitTransmitDone(u32 timeout_ms) FL_NO_EXCEPT override {
        (void)timeout_ms;
        // In the mock, transmission "completes" only when the test
        // calls simulateTransmitDone(). This method just reports the
        // current state without blocking (single-threaded design).
        return !mBusy;
    }

    bool isBusy() const FL_NO_EXCEPT override { return mBusy; }

    bool registerTransmitCallback(I2sEsp32DevTxDoneCallback cb,
                                  void *user_ctx) FL_NO_EXCEPT override {
        if (mFailRegisterCallback) {
            return false;
        }
        mCallback = cb;
        mCallbackUserCtx = user_ctx;
        return true;
    }

    const I2sEsp32DevPeripheralConfig &getConfig() const FL_NO_EXCEPT override {
        return mConfig;
    }

    //=== Mock simulation =====================================================

    void simulateTransmitDone() FL_NO_EXCEPT override {
        if (!mBusy) {
            return;
        }
        mBusy = false;
        if (mCallback != nullptr) {
            ++mCallbackInvocations;
            (*mCallback)(mCallbackUserCtx);
        }
    }

    u32 callbackInvocationCount() const FL_NO_EXCEPT override {
        return mCallbackInvocations;
    }

    //=== Error injection =====================================================

    void setInitializeFailure(bool fail) FL_NO_EXCEPT override {
        mFailInitialize = fail;
    }
    void setAllocateBufferFailure(bool fail) FL_NO_EXCEPT override {
        mFailAllocateBuffer = fail;
    }
    void setTransmitFailure(bool fail) FL_NO_EXCEPT override {
        mFailTransmit = fail;
    }
    void setRegisterCallbackFailure(bool fail) FL_NO_EXCEPT override {
        mFailRegisterCallback = fail;
    }

    //=== State inspection ====================================================

    const fl::vector<I2sEsp32DevTransmitRecord> &
    getTransmitHistory() const FL_NO_EXCEPT override {
        return mTxHistory;
    }
    size_t getTransmitCount() const FL_NO_EXCEPT override {
        return mTxHistory.size();
    }
    I2sEsp32DevTxDoneCallback registeredCallback() const FL_NO_EXCEPT override {
        return mCallback;
    }
    void *registeredCallbackUserCtx() const FL_NO_EXCEPT override {
        return mCallbackUserCtx;
    }
    size_t getAllocateBufferCount() const FL_NO_EXCEPT override {
        return mAllocateCount;
    }
    size_t getOutstandingBufferCount() const FL_NO_EXCEPT override {
        return mAllocations.size();
    }

    //=== Reset ==============================================================

    void reset() FL_NO_EXCEPT override {
        // Free every outstanding allocation.
        for (auto *buf : mAllocations) {
            if (buf) {
                fl::free(buf);
            }
        }
        mAllocations.clear();

        mConfig = I2sEsp32DevPeripheralConfig();
        mInitialized = false;
        mBusy = false;
        mTxHistory.clear();
        mTxSequence = 0;
        mCallback = nullptr;
        mCallbackUserCtx = nullptr;
        mAllocateCount = 0;
        mCallbackInvocations = 0;
        mFailInitialize = false;
        mFailAllocateBuffer = false;
        mFailTransmit = false;
        mFailRegisterCallback = false;
    }

  private:
    I2sEsp32DevPeripheralConfig mConfig;
    bool mInitialized;
    bool mBusy;

    fl::vector<I2sEsp32DevTransmitRecord> mTxHistory;
    u32 mTxSequence;

    I2sEsp32DevTxDoneCallback mCallback;
    void *mCallbackUserCtx;

    fl::vector<u8 *> mAllocations;
    size_t mAllocateCount;
    u32 mCallbackInvocations;

    bool mFailInitialize;
    bool mFailAllocateBuffer;
    bool mFailTransmit;
    bool mFailRegisterCallback;
};

//=============================================================================
// Singleton accessor
//=============================================================================

I2sPeripheralEsp32DevMock &I2sPeripheralEsp32DevMock::instance() FL_NO_EXCEPT {
    return Singleton<I2sPeripheralEsp32DevMockImpl>::instance();
}

} // namespace fl

#endif // host build
