#pragma once

#include "fl/channels/rx/config.h"
#include "fl/channels/rx.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/stdint.h"
#include "fl/stl/string.h"

namespace fl {

class RxChannel;
FASTLED_SHARED_PTR(RxChannel);

class RxChannel {
public:
    static RxChannelPtr create(const RxChannelConfig& config) FL_NO_EXCEPT;

    virtual ~RxChannel() FL_NO_EXCEPT;

    i32 id() const FL_NO_EXCEPT { return mId; }
    const fl::string& name() const FL_NO_EXCEPT { return mName; }
    int getPin() const FL_NO_EXCEPT;
    fl::string getEngineName() const FL_NO_EXCEPT;

    /// @brief The capture backend this channel was created with.
    /// Callers re-arming the channel via begin() should carry this
    /// value into the new config — begin() recreates the device when
    /// the backend differs, so a default-constructed config would
    /// silently swap an explicitly-selected backend back to
    /// PLATFORM_DEFAULT (FastLED#3576 Phase 3).
    RxBackend backend() const FL_NO_EXCEPT;

    bool begin(const RxChannelConfig& config) FL_NO_EXCEPT;
    void setConfig(const RxChannelConfig& config) FL_NO_EXCEPT;
    bool finished() const FL_NO_EXCEPT;
    RxWaitResult wait(u32 timeout_ms) FL_NO_EXCEPT;

    fl::result<u32, DecodeError> decode(const ChipsetTiming4Phase& timing,
                                        fl::span<u8> out) FL_NO_EXCEPT;
    size_t getRawEdgeTimes(fl::span<EdgeTime> out, size_t offset = 0) FL_NO_EXCEPT;
    bool injectEdges(fl::span<const EdgeTime> edges) FL_NO_EXCEPT;

    const RxChannelConfig& config() const FL_NO_EXCEPT { return mConfig; }

private:
    template<typename T, typename... Args>
    friend fl::shared_ptr<T> fl::make_shared(Args&&... args) FL_NO_EXCEPT;

    RxChannel(const RxChannelConfig& config, fl::shared_ptr<RxDevice> device) FL_NO_EXCEPT;

    static i32 nextId() FL_NO_EXCEPT;
    static fl::string makeName(i32 id, const fl::string& config_name) FL_NO_EXCEPT;

    RxChannel(const RxChannel&) FL_NO_EXCEPT = delete;
    RxChannel& operator=(const RxChannel&) FL_NO_EXCEPT = delete;
    RxChannel(RxChannel&&) FL_NO_EXCEPT = delete;
    RxChannel& operator=(RxChannel&&) FL_NO_EXCEPT = delete;

    const i32 mId;
    fl::string mName;
    RxChannelConfig mConfig;
    fl::shared_ptr<RxDevice> mDevice;
};

}  // namespace fl
