// IWYU pragma: private

#include "platforms/is_platform.h"
#if defined(FL_IS_ESP32)
#include "platforms/esp/32/feature_flags/enabled.h"
#if FASTLED_ESP32_HAS_RMT && !FASTLED_RMT5 && !FASTLED_ESP32_RMT5_ONLY_PLATFORM
#include "platforms/esp/32/drivers/rmt/rmt_4/rmt4_async_driver.h"
#if FL_RMT4_ASYNC
#include "platforms/esp/32/drivers/rmt/rmt_4/rmt4_frame_queue.h"
#include "platforms/esp/32/drivers/rmt/rmt_4/rmt_memory_manager_4.h"
#include "fl/stl/atomic.h"
#include "fl/stl/chrono.h"
#include "fl/stl/move.h"

FL_EXTERN_C_BEGIN
// IWYU pragma: begin_keep
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
// IWYU pragma: end_keep
FL_EXTERN_C_END

namespace fl {

struct Rmt4AsyncDriver::State {
    fl::weak_ptr<State> self;
    ChannelEngineRMT4Ptr hardware;
    Rmt4FrameQueue prepared;
    Rmt4FrameQueue submitted;
    SemaphoreHandle_t mutex = nullptr;
    SemaphoreHandle_t stopped = nullptr;
    TaskHandle_t task = nullptr;
    bool preparationFailed = false; // Producer mutex protects this batch flag.
    atomic_bool stop{false};
    atomic_bool failed{false};
    atomic_bool reportError{false};
    atomic_int status{DriverState::READY};

    explicit State(fl::shared_ptr<detail::IRMT4Peripheral> peripheral) FL_NO_EXCEPT
        : hardware(ChannelEngineRMT4::create(fl::move(peripheral))) {
        mutex = xSemaphoreCreateMutex();
        stopped = xSemaphoreCreateBinary();
    }
    ~State() FL_NO_EXCEPT {
        if (stopped) { vSemaphoreDelete(stopped); }
        if (mutex) { vSemaphoreDelete(mutex); }
    }
    void fail() FL_NO_EXCEPT {
        failed.store(true);
        reportError.store(true);
    }
};

Rmt4AsyncDriver::Rmt4AsyncDriver(fl::shared_ptr<detail::IRMT4Peripheral> peripheral) FL_NO_EXCEPT
    : mState(fl::make_shared<State>(fl::move(peripheral))) {
    if (mState) { mState->self = mState; }
    // Initialize the shared TX/RX allocator before a second task can use it.
    (void)RmtMemoryManager4::instance();
}

Rmt4AsyncDriver::~Rmt4AsyncDriver() FL_NO_EXCEPT {
    if (mState && mState->task) {
        mState->stop.store(true);
        xTaskNotifyGive(mState->task);
        // Worker tears down its SDK interrupt on the core that installed it.
        xSemaphoreTake(mState->stopped, portMAX_DELAY);
    }
}

bool Rmt4AsyncDriver::canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT {
    return data && data->isClockless();
}

void Rmt4AsyncDriver::enqueue(ChannelDataPtr data) FL_NO_EXCEPT {
    if (!mState || !mState->mutex || !mState->stopped || !mState->hardware) {
        if (mState) { mState->fail(); }
        return;
    }
    xSemaphoreTake(mState->mutex, portMAX_DELAY);
    if (data && !data->isInUse() && !mState->preparationFailed && mState->prepared.empty() &&
        mState->status.load() == DriverState::READY) {
        mState->failed.store(false);
        mState->reportError.store(false);
    }
    if (!mState->prepared.enqueue(data)) {
        mState->preparationFailed = true;
        mState->fail();
    }
    xSemaphoreGive(mState->mutex);
}

void Rmt4AsyncDriver::show() FL_NO_EXCEPT {
    if (!mState || !mState->mutex || !mState->stopped || !mState->hardware) {
        if (mState) { mState->fail(); }
        return;
    }
    if (!mState->task) {
        // Stack depth is bytes on ESP-IDF. Measure high-water before shrinking.
        constexpr u32 stackBytes = 2048;
        if (xTaskCreatePinnedToCore(&Rmt4AsyncDriver::run, "fl_rmt4", stackBytes,
                                   mState.get(), 2, &mState->task,
                                   xPortGetCoreID()) != pdPASS) {
            xSemaphoreTake(mState->mutex, portMAX_DELAY);
            mState->prepared.clear();
            mState->preparationFailed = false;
            mState->fail();
            xSemaphoreGive(mState->mutex);
            return;
        }
    }
    xSemaphoreTake(mState->mutex, portMAX_DELAY);
    if (!mState->prepared.empty()) {
        if (mState->submitted.appendFrom(mState->prepared)) {
            mState->status.store(DriverState::BUSY);
        } else {
            mState->prepared.clear();
            mState->fail();
        }
    }
    mState->preparationFailed = false;
    xSemaphoreGive(mState->mutex);
    xTaskNotifyGive(mState->task);
}

IChannelDriver::DriverState Rmt4AsyncDriver::poll() FL_NO_EXCEPT {
    if (!mState || mState->reportError.exchange(false)) {
        return DriverState::ERROR;
    }
    return DriverState(static_cast<DriverState::Value>(mState->status.load()));
}

bool Rmt4AsyncDriver::waitDone(u32 timeoutMs) FL_NO_EXCEPT {
    if (!mState) { return false; }
    const u32 started = fl::millis();
    while (mState->status.load() != DriverState::READY) {
        if (timeoutMs && fl::millis() - started >= timeoutMs) { return false; }
        vTaskDelay(1);
    }
    return !mState->failed.load();
}

u32 Rmt4AsyncDriver::workerStackHighWaterBytes() const FL_NO_EXCEPT {
    // ESP-IDF reports bytes, unlike upstream FreeRTOS stack-word units.
    return mState && mState->task ? uxTaskGetStackHighWaterMark(mState->task) : 0;
}

Rmt4TxDiagnostics Rmt4AsyncDriver::txDiagnostics(int channel) const FL_NO_EXCEPT {
    return mState && mState->hardware
        ? static_cast<const ChannelEngineRMT4Impl &>(*mState->hardware).txDiagnostics(channel)
        : Rmt4TxDiagnostics();
}

void Rmt4AsyncDriver::run(void* argument) FL_NO_EXCEPT {
    // SDK callback argument is only a bootstrap address. Retain shared state
    // before doing work; the wrapper joins this task before releasing it.
    auto state = static_cast<State*>(argument)->self.lock();
    if (!state) { vTaskDelete(nullptr); return; }
    bool busy = false;
    while (true) {
        ulTaskNotifyTake(pdTRUE, busy ? 1 : portMAX_DELAY);
        if (state->stop.load()) { break; }
        bool submitted = false;
        xSemaphoreTake(state->mutex, portMAX_DELAY);
        while (!state->submitted.empty()) {
            state->hardware->enqueue(state->submitted.take(0));
            submitted = true;
        }
        xSemaphoreGive(state->mutex);
        if (submitted) { state->hardware->show(); }
        auto result = state->hardware->poll();
        if (result.state == DriverState::ERROR) { state->fail(); }
        busy = result.state != DriverState::READY;
        xSemaphoreTake(state->mutex, portMAX_DELAY);
        // A racing producer can have submitted the next batch while SDK work
        // ran. Never publish READY until those retained frames are consumed.
        if (!state->submitted.empty()) {
            busy = true;
            state->status.store(DriverState::BUSY);
        } else {
            state->status.store(busy ? DriverState::DRAINING : DriverState::READY);
        }
        xSemaphoreGive(state->mutex);
    }
    // SDK teardown and frame release happen before acknowledging shutdown.
    state->hardware.reset();
    state->prepared.clear();
    state->submitted.clear();
    xSemaphoreGive(state->stopped);
    state.reset();
    vTaskDelete(nullptr);
}

} // namespace fl
#endif
#endif
#endif
