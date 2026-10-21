#pragma once

// IWYU pragma: private

#include "platforms/esp/32/drivers/rmt/rmt_4/rmt4_config.h"
#include "platforms/esp/32/drivers/rmt/rmt_4/channel_driver_rmt4.h"

#if FL_RMT4_ASYNC
namespace fl {

/// Keeps SDK hardware ownership on one task while legacy show() returns.
class Rmt4AsyncDriver final : public ChannelEngineRMT4 {
public:
    explicit Rmt4AsyncDriver(fl::shared_ptr<detail::IRMT4Peripheral> peripheral) FL_NO_EXCEPT;
    ~Rmt4AsyncDriver() FL_NO_EXCEPT override;
    bool canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT override;
    void enqueue(ChannelDataPtr data) FL_NO_EXCEPT override;
    void show() FL_NO_EXCEPT override;
    DriverState poll() FL_NO_EXCEPT override;
    bool waitDone(u32 timeoutMs = 1000) FL_NO_EXCEPT override;
    /// Minimum unused worker stack in bytes, or zero before first submission.
    u32 workerStackHighWaterBytes() const FL_NO_EXCEPT;
    Rmt4TxDiagnostics txDiagnostics(int channel = -1) const FL_NO_EXCEPT;

private:
    struct State;
    fl::shared_ptr<State> mState;
    static void run(void* argument) FL_NO_EXCEPT;
};

} // namespace fl
#endif
