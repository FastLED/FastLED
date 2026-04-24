#include "fl/channels/rx/channel.h"

#include "fl/stl/atomic.h"
#include "fl/stl/string.h"

namespace fl {

namespace {

static RxConfig toRxConfig(const RxChannelConfig& config) FL_NOEXCEPT {
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

static fl::shared_ptr<RxDevice> createBackendDevice(const RxChannelConfig& config) FL_NOEXCEPT {
    switch (config.backend) {
    case RxBackend::PLATFORM_DEFAULT:
        return RxDevice::create<RxDeviceType::PLATFORM_DEFAULT>(config.pin);
    case RxBackend::RMT:
        return RxDevice::create<RxDeviceType::RMT>(config.pin);
    case RxBackend::ISR:
        return RxDevice::create<RxDeviceType::ISR>(config.pin);
    case RxBackend::FLEXPWM:
        return RxDevice::create<RxDeviceType::FLEXPWM>(config.pin);
    }
    return RxDevice::create<RxDeviceType::PLATFORM_DEFAULT>(config.pin);
}

}  // namespace

i32 RxChannel::nextId() FL_NOEXCEPT {
    static fl::atomic<i32> gNextRxChannelId(0);
    return gNextRxChannelId.fetch_add(1);
}

fl::string RxChannel::makeName(i32 id, const fl::string& config_name) FL_NOEXCEPT {
    if (!config_name.empty()) {
        return config_name;
    }
    return "RxChannel_" + fl::to_string(static_cast<i64>(id));
}

RxChannelPtr RxChannel::create(const RxChannelConfig& config) FL_NOEXCEPT {
    return fl::make_shared<RxChannel>(config, createBackendDevice(config));
}

RxChannel::RxChannel(const RxChannelConfig& config, fl::shared_ptr<RxDevice> device) FL_NOEXCEPT
    : mId(nextId())
    , mName(makeName(mId, config.name))
    , mConfig(config)
    , mDevice(fl::move(device)) {
}

RxChannel::~RxChannel() FL_NOEXCEPT = default;

int RxChannel::getPin() const FL_NOEXCEPT {
    return mDevice ? mDevice->getPin() : mConfig.pin;
}

fl::string RxChannel::getEngineName() const FL_NOEXCEPT {
    if (!mConfig.affinity.empty()) {
        return mConfig.affinity;
    }
    return mDevice ? fl::string(mDevice->name()) : fl::string();
}

bool RxChannel::begin(const RxChannelConfig& config) FL_NOEXCEPT {
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

void RxChannel::setConfig(const RxChannelConfig& config) FL_NOEXCEPT {
    if (!config.name.empty()) {
        mName = config.name;
    }
    mConfig = config;
}

bool RxChannel::finished() const FL_NOEXCEPT {
    return mDevice ? mDevice->finished() : true;
}

RxWaitResult RxChannel::wait(u32 timeout_ms) FL_NOEXCEPT {
    return mDevice ? mDevice->wait(timeout_ms) : RxWaitResult::TIMEOUT;
}

fl::result<u32, DecodeError> RxChannel::decode(const ChipsetTiming4Phase& timing,
                                               fl::span<u8> out) FL_NOEXCEPT {
    if (!mDevice) {
        return fl::result<u32, DecodeError>::failure(DecodeError::INVALID_ARGUMENT);
    }
    return mDevice->decode(timing, out);
}

size_t RxChannel::getRawEdgeTimes(fl::span<EdgeTime> out, size_t offset) FL_NOEXCEPT {
    return mDevice ? mDevice->getRawEdgeTimes(out, offset) : 0;
}

bool RxChannel::injectEdges(fl::span<const EdgeTime> edges) FL_NOEXCEPT {
    return mDevice ? mDevice->injectEdges(edges) : false;
}

}  // namespace fl
