/// @file i2s_peripheral_esp32dev_mock.h
/// @brief Host-side mock for `II2sPeripheralEsp32Dev` (#3471).
///
/// Same shape as `Rmt4PeripheralMock` (#3463), `I2sLcdCamPeripheralMock`,
/// and every other peer peripheral mock in the tree: abstract public
/// class + concrete `Impl` private to the `.cpp.hpp`, singleton via
/// `fl::Singleton<Impl>`, **single-threaded by design** (no
/// `std::thread`, no async callbacks — tests advance simulation
/// manually via `simulateTransmitDone()`), error-injection knobs for
/// each fallible entry point.
///
/// The engine's channel-management state machine can be exercised
/// against this mock without touching real hardware.

#pragma once

// IWYU pragma: private

// No platform guard — the mock runs on every host build.

#include "fl/stl/cstddef.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/stdint.h"
#include "fl/stl/vector.h"
#include "platforms/esp/32/drivers/i2s/ii2s_peripheral_esp32dev.h"

namespace fl {

//=============================================================================
// Recorded state for tests
//=============================================================================

/// @brief A copy of one transmit() call — the pixel data, the size,
///        and a monotonically-incrementing sequence number. Tests
///        assert against the history vector to verify the engine
///        packed the right bytes.
struct I2sEsp32DevTransmitRecord {
    fl::vector<u8> buffer_copy;
    size_t size_bytes;
    u32 sequence;

    I2sEsp32DevTransmitRecord() FL_NO_EXCEPT : buffer_copy(),
                                               size_bytes(0),
                                               sequence(0) {}
};

//=============================================================================
// Mock peripheral
//=============================================================================

class I2sPeripheralEsp32DevMock : public II2sPeripheralEsp32Dev {
  public:
    /// @brief Singleton accessor. Mirrors the RMT5, RMT4, UART, and
    ///        S3 LCD_CAM mock patterns — one instance shared across
    ///        the process, tests reset it between cases via
    ///        `reset()`. Wiring the mock into an engine that expects
    ///        `fl::shared_ptr<II2sPeripheralEsp32Dev>` is done in the
    ///        test TU via a tiny forwarding `MockWrapper` (see
    ///        `tests/.../channel_driver_i2s.cpp` for the canonical
    ///        example).
    static I2sPeripheralEsp32DevMock &instance() FL_NO_EXCEPT;

    ~I2sPeripheralEsp32DevMock() override = default;

    // II2sPeripheralEsp32Dev — declared abstract here; the concrete
    // Impl provides the bodies.
    bool initialize(const I2sEsp32DevPeripheralConfig &cfg)
        FL_NO_EXCEPT override = 0;
    void deinitialize() FL_NO_EXCEPT override = 0;
    bool isInitialized() const FL_NO_EXCEPT override = 0;

    u8 *allocateBuffer(size_t size_bytes) FL_NO_EXCEPT override = 0;
    void freeBuffer(u8 *buffer) FL_NO_EXCEPT override = 0;

    bool transmit(const u8 *buffer,
                  size_t size_bytes) FL_NO_EXCEPT override = 0;
    bool waitTransmitDone(u32 timeout_ms) FL_NO_EXCEPT override = 0;
    bool isBusy() const FL_NO_EXCEPT override = 0;

    bool registerTransmitCallback(I2sEsp32DevTxDoneCallback cb,
                                  void *user_ctx) FL_NO_EXCEPT override = 0;

    const I2sEsp32DevPeripheralConfig &
    getConfig() const FL_NO_EXCEPT override = 0;

    //=========================================================================
    // Mock-only simulation API (no threads)
    //=========================================================================

    /// @brief Synchronously invoke the stored `transmit-done`
    ///        callback on the calling thread, mark the peripheral
    ///        idle, and return. Test-only.
    virtual void simulateTransmitDone() FL_NO_EXCEPT = 0;

    /// @brief Number of times the stored callback has been invoked
    ///        since the most recent `reset()`.
    virtual u32 callbackInvocationCount() const FL_NO_EXCEPT = 0;

    //=========================================================================
    // Error-injection knobs
    //=========================================================================

    virtual void setInitializeFailure(bool fail) FL_NO_EXCEPT = 0;
    virtual void setAllocateBufferFailure(bool fail) FL_NO_EXCEPT = 0;
    virtual void setTransmitFailure(bool fail) FL_NO_EXCEPT = 0;
    virtual void setRegisterCallbackFailure(bool fail) FL_NO_EXCEPT = 0;

    //=========================================================================
    // State inspection
    //=========================================================================

    /// @brief Copy of every transmit() call in chronological order.
    virtual const fl::vector<I2sEsp32DevTransmitRecord> &
    getTransmitHistory() const FL_NO_EXCEPT = 0;

    /// @brief Number of transmit() calls since the last reset.
    virtual size_t getTransmitCount() const FL_NO_EXCEPT = 0;

    /// @brief Registered callback and its user_ctx (nullptr if none).
    virtual I2sEsp32DevTxDoneCallback
    registeredCallback() const FL_NO_EXCEPT = 0;
    virtual void *registeredCallbackUserCtx() const FL_NO_EXCEPT = 0;

    /// @brief Number of `allocateBuffer()` calls made since reset.
    virtual size_t getAllocateBufferCount() const FL_NO_EXCEPT = 0;

    /// @brief Number of currently-outstanding `allocateBuffer()`
    ///        allocations (allocated minus freed). Useful to catch
    ///        leaks in the engine's buffer lifetime.
    virtual size_t getOutstandingBufferCount() const FL_NO_EXCEPT = 0;

    //=========================================================================
    // Reset
    //=========================================================================

    /// @brief Return the mock to a clean state for the next test.
    ///        Clears history, allocations, error flags, and any
    ///        registered callback.
    virtual void reset() FL_NO_EXCEPT = 0;
};

} // namespace fl
