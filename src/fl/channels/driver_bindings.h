#pragma once

/// @file driver_bindings.h
/// @brief Opt-in, link-on-use channel driver bindings for `FastLED.add(cfg)`.
///
/// `FastLED.enable<fl::Bus::X>()` self-adds one static `DriverBinding` node to
/// the `DriverBindings` singleton. The node holds only a function pointer to
/// that bus's registration; the driver itself is constructed the first time
/// `FastLED.add(cfg)` needs it. Nothing else names the registration, so a bus
/// that is never enabled never links (`--gc-sections` drops it).
///
/// `FastLED.add(cfg)` binds the node for the requested bus (or every node for
/// `Bus::AUTO`) and, when no driver can serve the channel, warns once a second
/// instead of linking every driver. `FastLED.enableAllDrivers()` remains the
/// catch-all that registers every driver on the platform.
///
/// @code
///   FastLED.enable<fl::Bus::RMT>().enable<fl::Bus::FLEX_IO>();
///   FastLED.add(cfg);  // constructs RMT/FLEX_IO here, on first use
/// @endcode

#include "fl/channels/bus.h"
#include "fl/channels/bus_traits.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// One enabled bus. Static storage, linked intrusively; never heap.
struct DriverBinding {
    Bus bus;
    u8 which;
    void (*bind)();            ///< registers the driver with ChannelManager
    DriverBinding* next;
    bool listed;
};

/// Process-wide list of enabled buses.
class DriverBindings {
public:
    static DriverBindings& instance() FL_NO_EXCEPT;

    /// Self-add a node. Idempotent: a node already listed is ignored.
    void push(DriverBinding& node) FL_NO_EXCEPT;

    /// Bind (register) every node matching `bus`/`which`, or every node when
    /// `bus` is `Bus::AUTO`. Returns true when at least one node matched.
    bool bind(Bus bus, u8 which) FL_NO_EXCEPT;

    bool empty() const FL_NO_EXCEPT { return mHead == nullptr; }

private:
    DriverBinding* mHead = nullptr;
};

namespace detail {

template<Bus B, typename Chipset, u8 Which>
struct BusBinding {
    static constexpr Bus kBus =
        B == Bus::AUTO ? DefaultBus<Chipset>::value : B;

    static void bind() FL_NO_EXCEPT {
        enable_clockless_for<Chipset, BusTraits<kBus, Which>>::apply();  // #4793
        BusTraits<kBus, Which>::registerWithManager();
    }

    static DriverBinding& node() FL_NO_EXCEPT {
        static DriverBinding sNode = {kBus, Which, &BusBinding::bind,
                                      nullptr, false};  // okay static in header
        return sNode;
    }
};

/// True when the platform specializes `DefaultBus<Chipset>`. Platforms with
/// no default (e.g. Zephyr, or LPC11xx SPI) leave it incomplete.
template<typename Chipset, typename = void>
struct HasDefaultBus : fl::false_type {};

template<typename Chipset>
struct HasDefaultBus<Chipset, decltype(void(DefaultBus<Chipset>::value))>
    : fl::true_type {};

template<typename Chipset>
inline bool bindDefaultDriverImpl(fl::true_type) FL_NO_EXCEPT {
    BusBinding<Bus::AUTO, Chipset, 0>::bind();
    return true;
}

template<typename Chipset>
inline bool bindDefaultDriverImpl(fl::false_type) FL_NO_EXCEPT {
    return false;  // no platform default: nothing to bind
}

/// Bind the platform default driver for `Chipset`, if one exists. Returns
/// false (and links nothing) where `DefaultBus<Chipset>` is incomplete.
template<typename Chipset>
inline bool bindDefaultDriver() FL_NO_EXCEPT {
    return bindDefaultDriverImpl<Chipset>(HasDefaultBus<Chipset>());
}

}  // namespace detail

/// Enable one bus for `FastLED.add(cfg)`. `Bus::AUTO` names the platform
/// default for `Chipset`. Links that bus only; construction is deferred.
template<Bus B, typename Chipset = ClocklessChipset, u8 Which = 0>
inline void bindDriver() FL_NO_EXCEPT {
    DriverBindings::instance().push(
        detail::BusBinding<B, Chipset, Which>::node());
}

}  // namespace fl
