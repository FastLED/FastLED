/// @file i2s_peripheral_esp32dev_esp.h
/// @brief Real-hardware `II2sPeripheralEsp32Dev` for classic ESP32 /
///        IDF 4.x (#3474 Stage 2).
///
/// Wraps ESP-IDF v4's `driver/i2s.h` DMA-driven TX path. The IDF
/// driver already installs its own DMA + ISR chain for the queue-
/// backed FIFO; this class exposes that machinery through the
/// `II2sPeripheralEsp32Dev` interface so `ChannelEngineI2sEsp32Dev`
/// (Stage 1 #3473) can use it interchangeably with the singleton
/// host mock.
///
/// ## Async completion story
///
/// The IDF v4 I2S driver doesn't expose a per-buffer completion
/// callback the way we want to. Instead we treat the peripheral as
/// "busy = FIFO not yet drained": every `transmit()` posts the whole
/// buffer to the internal DMA queue via `i2s_write()`, then a lazy
/// helper task blocks on `i2s_wait_tx_done()` and fires the
/// registered callback once the FIFO reports empty. The engine
/// contract (register callback once at construction; wait for
/// callback to flip its "completed" flag; `poll()` drops back to
/// READY) still holds — the callback just runs on a background
/// FreeRTOS task rather than in an ISR. From the engine's
/// perspective the semantics are identical.
///
/// A follow-up PR can replace the IDF driver with direct
/// `I2S0`/`I2S1` register + `intr_alloc()` handling to get true
/// ISR-context completion (matching the RMT4 TX path). The
/// peripheral interface is stable across that switch — only the
/// implementation of this class changes.

#pragma once

// IWYU pragma: private

#include "fl/stl/compiler_control.h"
#include "platforms/is_platform.h"

#ifdef FL_IS_ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

// The classic-ESP32 I2S parallel-out driver is only available on the
// original ESP32 (`FL_IS_ESP_32DEV`) with IDF < 6. `FASTLED_ESP32_HAS_I2S`
// already captures the intersection.
#if FASTLED_ESP32_HAS_I2S

#include "fl/stl/noexcept.h"
#include "platforms/esp/32/drivers/i2s/ii2s_peripheral_esp32dev.h"

namespace fl {

/// @brief Real-hardware I2S peripheral for classic ESP32 (IDF 4.x).
///
/// Stateful — one instance per peripheral. Not copyable, not movable.
/// Construction is cheap (no hardware touched until `initialize()`);
/// destruction cleans up the IDF driver + the completion helper task.
class I2sPeripheralEsp32DevEsp : public II2sPeripheralEsp32Dev {
  public:
    I2sPeripheralEsp32DevEsp() FL_NO_EXCEPT;
    ~I2sPeripheralEsp32DevEsp() override;

    I2sPeripheralEsp32DevEsp(const I2sPeripheralEsp32DevEsp &) = delete;
    I2sPeripheralEsp32DevEsp &
    operator=(const I2sPeripheralEsp32DevEsp &) = delete;

    bool
    initialize(const I2sEsp32DevPeripheralConfig &cfg) FL_NO_EXCEPT override;
    void deinitialize() FL_NO_EXCEPT override;
    bool isInitialized() const FL_NO_EXCEPT override;

    u8 *allocateBuffer(size_t size_bytes) FL_NO_EXCEPT override;
    void freeBuffer(u8 *buffer) FL_NO_EXCEPT override;

    bool transmit(const u8 *buffer, size_t size_bytes) FL_NO_EXCEPT override;
    bool waitTransmitDone(u32 timeout_ms) FL_NO_EXCEPT override;
    bool isBusy() const FL_NO_EXCEPT override;

    bool registerTransmitCallback(I2sEsp32DevTxDoneCallback cb,
                                  void *user_ctx) FL_NO_EXCEPT override;

    const I2sEsp32DevPeripheralConfig &getConfig() const FL_NO_EXCEPT override;

  private:
    I2sEsp32DevPeripheralConfig mConfig;
    bool mInitialized;
    volatile bool mBusy;
    I2sEsp32DevTxDoneCallback mCallback;
    void *mCallbackUserCtx;
};

} // namespace fl

#endif // FASTLED_ESP32_HAS_I2S
#endif // FL_IS_ESP32
