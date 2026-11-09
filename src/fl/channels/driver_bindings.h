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

template<typename...> struct BindingVoid { using type = void; };

// `Bus::AUTO` resolves through `DefaultBus<Chipset>`, which some platforms
// (RP2040, SAMD, ...) leave undefined for a chipset family. There it stays
// AUTO, and enabling it is a no-op: those platforms' default drivers come
// from `platforms::initChannelDrivers()`.
template<Bus B, typename Chipset, typename = void>
struct ResolveBus { static constexpr Bus value = B; };

template<typename Chipset>
struct ResolveBus<Bus::AUTO, Chipset,
    typename BindingVoid<decltype(DefaultBus<Chipset>::value)>::type> {
    static constexpr Bus value = DefaultBus<Chipset>::value;
};

template<Bus B, typename Chipset, u8 Which, bool RESOLVED = (B != Bus::AUTO)>
struct BusBinding {
    static constexpr Bus kBus = B;

    static void bind() FL_NO_EXCEPT {
        enable_clockless_for<Chipset, BusTraits<kBus, Which>>::apply();  // #4793
        BusTraits<kBus, Which>::registerWithManager();
    }

    static void push(DriverBindings& list) FL_NO_EXCEPT {
        static DriverBinding sNode = {kBus, Which, &BusBinding::bind,
                                      nullptr, false};  // okay static in header
        list.push(sNode);
    }
};

/// No platform default for this chipset family: nothing to link.
template<Bus B, typename Chipset, u8 Which>
struct BusBinding<B, Chipset, Which, false> {
    static constexpr Bus kBus = B;
    static void push(DriverBindings&) FL_NO_EXCEPT {}
};

}  // namespace detail

/// Enable one bus for `FastLED.add(cfg)`. `Bus::AUTO` names the platform
/// default for `Chipset` (a no-op where the platform defines none). Links that
/// bus only; construction is deferred.
template<Bus B, typename Chipset = ClocklessChipset, u8 Which = 0>
inline void bindDriver() FL_NO_EXCEPT {
    constexpr Bus kBus = detail::ResolveBus<B, Chipset>::value;
    detail::BusBinding<kBus, Chipset, Which>::push(DriverBindings::instance());
}

}  // namespace fl
