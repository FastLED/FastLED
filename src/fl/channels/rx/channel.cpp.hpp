#include "fl/channels/rx/channel.h"

#include "fl/log/log.h"
#include "fl/stl/atomic.h"
#include "fl/stl/string.h"

namespace fl {

namespace {

static RxConfig toRxConfig(const RxChannelConfig& config) FL_NO_EXCEPT {
    RxConfig out;
    out.buffer_size = config.edge_capacity;
    out.hz = config.hz;
    out.signal_range_min_ns = config.signal_range_min_ns;
    out.signal_range_max_ns = config.signal_range_max_ns;
    out.skip_signals = config.skip_signals;
    out.start_low = config.start_low;
    out.io_loop_back = config.io_loop_back;
    out.use_dma = config.use_dma;
    return out;
}

static fl::shared_ptr<RxDevice> createBackendDevice(const RxChannelConfig& config) FL_NO_EXCEPT {
    switch (config.backend) {
    case RxBackend::PLATFORM_DEFAULT:
        return RxDevice::create<RxDeviceType::PLATFORM_DEFAULT>(config.pin);
    case RxBackend::RMT:
        return RxDevice::create<RxDeviceType::RMT>(config.pin);
    case RxBackend::ISR:
        return RxDevice::create<RxDeviceType::ISR>(config.pin);
    case RxBackend::FLEXPWM:
        return RxDevice::create<RxDeviceType::FLEXPWM>(config.pin);
    case RxBackend::FLEXIO:
        return RxDevice::create<RxDeviceType::FLEXIO>(config.pin);
    case RxBackend::LPC_SCT_CAPTURE:
        return RxDevice::create<RxDeviceType::LPC_SCT_CAPTURE>(config.pin);
    case RxBackend::I2S_RX:
        return RxDevice::create<RxDeviceType::I2S_RX>(config.pin);
    }
    return RxDevice::create<RxDeviceType::PLATFORM_DEFAULT>(config.pin);
}

}  // namespace

i32 RxChannel::nextId() FL_NO_EXCEPT {
    static fl::atomic<i32> gNextRxChannelId(0);
    return gNextRxChannelId.fetch_add(1);
}

fl::string RxChannel::makeName(i32 id, const fl::string& config_name) FL_NO_EXCEPT {
    if (!config_name.empty()) {
        return config_name;
    }
    // Mirrors Channel::makeName's release-build gate (#2942/#2943).
    // mName is consumed only by FL_WARN/FL_ERROR sites that collapse to
    // do-while-0 when FASTLED_LOG_VERBOSITY=0 (NDEBUG default per #2890),
    // so the string + fl::to_string<i64> + heap concat run for nothing
    // in release. See #2954.
#if FASTLED_LOG_RUNTIME_ENABLED
    return "RxChannel_" + fl::to_string(static_cast<i64>(id));
#else
    (void)id;
    return {};
#endif
}

RxChannelPtr RxChannel::create(const RxChannelConfig& config) FL_NO_EXCEPT {
    return fl::make_shared<RxChannel>(config, createBackendDevice(config));
}

RxChannel::RxChannel(const RxChannelConfig& config, fl::shared_ptr<RxDevice> device) FL_NO_EXCEPT
    : mId(nextId())
    , mName(makeName(mId, config.name))
    , mConfig(config)
    , mDevice(fl::move(device)) {
}

RxChannel::~RxChannel() FL_NO_EXCEPT = default;

int RxChannel::getPin() const FL_NO_EXCEPT {
    return mDevice ? mDevice->getPin() : mConfig.pin;
}

RxBackend RxChannel::backend() const FL_NO_EXCEPT {
    return mConfig.backend;
}

fl::string RxChannel::getEngineName() const FL_NO_EXCEPT {
    if (!mConfig.affinity.empty()) {
        return mConfig.affinity;
    }
    return mDevice ? fl::string(mDevice->name()) : fl::string();
}

bool RxChannel::begin(const RxChannelConfig& config) FL_NO_EXCEPT {
    bool recreate_device = !mDevice ||
                           config.pin != mConfig.pin ||
                           config.backend != mConfig.backend;
    if (recreate_device) {
        mDevice = createBackendDevice(config);
    }
    if (!config.name.empty()) {
        mName = config.name;
    }
    mConfig = config;
    return mDevice ? mDevice->begin(toRxConfig(mConfig)) : false;
}

void RxChannel::setConfig(const RxChannelConfig& config) FL_NO_EXCEPT {
    if (!config.name.empty()) {
        mName = config.name;
    }
    mConfig = config;
}

bool RxChannel::finished() const FL_NO_EXCEPT {
    return mDevice ? mDevice->finished() : true;
}

RxWaitResult RxChannel::wait(u32 timeout_ms) FL_NO_EXCEPT {
    return mDevice ? mDevice->wait(timeout_ms) : RxWaitResult::TIMEOUT;
}

fl::result<u32, DecodeError> RxChannel::decode(const ChipsetTiming4Phase& timing,
                                               fl::span<u8> out) FL_NO_EXCEPT {
    if (!mDevice) {
        return fl::result<u32, DecodeError>::failure(DecodeError::INVALID_ARGUMENT);
    }
    return mDevice->decode(timing, out);
}

size_t RxChannel::getRawEdgeTimes(fl::span<EdgeTime> out, size_t offset) FL_NO_EXCEPT {
    return mDevice ? mDevice->getRawEdgeTimes(out, offset) : 0;
}

bool RxChannel::injectEdges(fl::span<const EdgeTime> edges) FL_NO_EXCEPT {
    return mDevice ? mDevice->injectEdges(edges) : false;
}

}  // namespace fl
