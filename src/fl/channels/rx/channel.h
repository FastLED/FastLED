#pragma once

#include "fl/channels/rx/config.h"
#include "fl/rx_device.h"
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
    static RxChannelPtr create(const RxChannelConfig& config) FL_NOEXCEPT;

    virtual ~RxChannel() FL_NOEXCEPT;

    i32 id() const FL_NOEXCEPT { return mId; }
    const fl::string& name() const FL_NOEXCEPT { return mName; }
    int getPin() const FL_NOEXCEPT;
    fl::string getEngineName() const FL_NOEXCEPT;

    bool begin(const RxChannelConfig& config) FL_NOEXCEPT;
    void setConfig(const RxChannelConfig& config) FL_NOEXCEPT;
    bool finished() const FL_NOEXCEPT;
    RxWaitResult wait(u32 timeout_ms) FL_NOEXCEPT;

    fl::result<u32, DecodeError> decode(const ChipsetTiming4Phase& timing,
                                        fl::span<u8> out) FL_NOEXCEPT;
    size_t getRawEdgeTimes(fl::span<EdgeTime> out, size_t offset = 0) FL_NOEXCEPT;
    bool injectEdges(fl::span<const EdgeTime> edges) FL_NOEXCEPT;

    const RxChannelConfig& config() const FL_NOEXCEPT { return mConfig; }

private:
    template<typename T, typename... Args>
    friend fl::shared_ptr<T> fl::make_shared(Args&&... args) FL_NOEXCEPT;

    RxChannel(const RxChannelConfig& config, fl::shared_ptr<RxDevice> device) FL_NOEXCEPT;

    static i32 nextId() FL_NOEXCEPT;
    static fl::string makeName(i32 id, const fl::string& config_name) FL_NOEXCEPT;

    RxChannel(const RxChannel&) FL_NOEXCEPT = delete;
    RxChannel& operator=(const RxChannel&) FL_NOEXCEPT = delete;
    RxChannel(RxChannel&&) FL_NOEXCEPT = delete;
    RxChannel& operator=(RxChannel&&) FL_NOEXCEPT = delete;

    const i32 mId;
    fl::string mName;
    RxChannelConfig mConfig;
    fl::shared_ptr<RxDevice> mDevice;
};

}  // namespace fl
