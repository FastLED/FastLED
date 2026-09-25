#pragma once

// IWYU pragma: private

/// @file clockless_arm_stm32.h
/// @brief STM32 legacy clockless controller routed through the slim bridge.
///
/// `ClocklessController` is a `fl::SlimBridgeController` (issue #4594): the
/// legacy `addLeds<>()` path encodes pixels into a `ChannelData` buffer and
/// hands it to a per-pin `ClocklessStm32Driver`. The driver keeps the
/// existing cycle-counted bit-bang cores (M0 asm core from `m0clockless.h`,
/// DWT/CYCCNT loop on Cortex-M3/M4/M7) as the byte-emitting engine.
///
/// The driver registers itself with `ChannelManager::registry()` from the
/// bridge constructor, so it is linked only when a sketch instantiates a
/// clockless controller (#4630 used-driver-only linking).

#include "fl/chipsets/timing_traits.h"
#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/channels/manager.h"
#include "fl/channels/slim_bridge_controller.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "fl/stl/has_include.h"
#include "eorder.h"
#include "fastled_delay.h"
#include "platforms/arm/stm32/interrupts_stm32_inline.h"
#include "platforms/arm/stm32/core_detection.h"
#include "platforms/arm/stm32/is_stm32.h"

// Get CMSIS DWT/CoreDebug registers from framework or fallback
#if FL_HAS_INCLUDE("stm32_def.h")
    // STM32duino core - stm32_def.h includes device header which includes core_cmX.h
    // IWYU pragma: begin_keep
    #include <stm32_def.h>
    // IWYU pragma: end_keep
#else
    // Reuse already included CMSIS (e.g. Zephyr); otherwise use libmaple fallback.
    #include "platforms/arm/stm32/cm3_regs.h"
#include "fl/stl/noexcept.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER
#endif

#include "platforms/arm/is_arm.h"
#include "platforms/arm/common/m0clockless.h"

namespace fl {

namespace stm32_detail {
// Hands out a distinct id per ClocklessStm32Driver specialization.
inline u32 nextClocklessTypeId() FL_NO_EXCEPT;
}  // namespace stm32_detail
// Definition for a single channel clockless controller for the stm32 family of chips, like that used in the spark core
// See clockless.h for detailed info on how the template parameters are used.
// Member definitions live in clockless_arm_stm32_impl.h (included by fastled_arm_stm32.h).

#define FL_CLOCKLESS_CONTROLLER_DEFINED 1

#if defined(FL_IS_ARM_M0_PLUS) || defined(FL_IS_ARM_M0) || defined(__ARM_ARCH_6M__)

/// @brief Blocking single-pin clockless driver for STM32 M0/M0+ (asm core).
///
/// The input buffer is already colour-ordered, scaled and dithered by the
/// bridge's `PixelIterator`, so the asm core runs with identity scale and
/// zero dither and simply shifts the bytes out.
template <int DATA_PIN, typename TIMING, int WAIT_TIME, int XTRA0 = 0>
class ClocklessStm32Driver : public IChannelDriver {
    // Stable id unique to this template specialization.
    static u32 typeId() FL_NO_EXCEPT;

public:
    ClocklessStm32Driver() FL_NO_EXCEPT;

    bool canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT override;
    void enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT override;
    void show() FL_NO_EXCEPT override;
    DriverState poll() FL_NO_EXCEPT override;
    /// Unique per template specialization so controllers sharing a pin with
    /// different timings keep separate drivers.
    fl::string getName() const FL_NO_EXCEPT override;
    Capabilities getCapabilities() const FL_NO_EXCEPT override;

private:
    static void initData(M0ClocklessData* data) FL_NO_EXCEPT;

    /// Emit `len` raw bytes. The asm core consumes 3-byte groups, so a
    /// trailing 1-2 byte remainder (RGBW strips) is zero-padded.
    /// @return 0 if an interrupt overran the frame, nonzero on success.
    static u32 sendBytes(const u8* bytes, u32 len) FL_NO_EXCEPT;

    fl::vector<ChannelDataPtr> mEnqueued;
    CMinWait<WAIT_TIME> mWait;
    bool mPinReady;
};

#else

#if defined(FL_IS_STM32_F2)
// The photon runs faster than the others
#define ADJ 8
#else
#define ADJ 20
#endif

#define _CYCCNT (*(volatile u32*)(0xE0001004UL))

/// @brief Blocking single-pin clockless driver for STM32 Cortex-M3/M4/M7
/// (DWT cycle-counter bit-bang core).
///
/// The input buffer is already colour-ordered, scaled and dithered (and RGBW
/// expanded) by the bridge's `PixelIterator`; the core only shifts bytes out.
template <int DATA_PIN, typename TIMING, int WAIT_TIME, int XTRA0 = 0>
class ClocklessStm32Driver : public IChannelDriver {
    // Stable id unique to this template specialization.
    static u32 typeId() FL_NO_EXCEPT;

    typedef typename FastPin<DATA_PIN>::port_ptr_t data_ptr_t;
    typedef typename FastPin<DATA_PIN>::port_t data_t;

public:
    ClocklessStm32Driver() FL_NO_EXCEPT;

    bool canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT override;
    void enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT override;
    void show() FL_NO_EXCEPT override;
    DriverState poll() FL_NO_EXCEPT override;
    /// Unique per template specialization so controllers sharing a pin with
    /// different timings keep separate drivers.
    fl::string getName() const FL_NO_EXCEPT override;
    Capabilities getCapabilities() const FL_NO_EXCEPT override;

private:
    void showBytes(const u8* bytes, u32 len) FL_NO_EXCEPT;

    template<int BITS> __attribute__ ((always_inline))
    inline static void writeBits(
        FASTLED_REGISTER u32 & next_mark, FASTLED_REGISTER data_ptr_t port,
        FASTLED_REGISTER data_t hi, FASTLED_REGISTER data_t lo, FASTLED_REGISTER u8 & b,
        u32 t1_clocks, u32 t1t2_clocks, u32 t1t2t3_clocks) FL_NO_EXCEPT;

    static u32 showRGBInternal(
            const u8* bytes, u32 len,
            u32 t1_clocks, u32 t2_clocks, u32 t3_clocks, u32 clks_per_us) FL_NO_EXCEPT;

    fl::vector<ChannelDataPtr> mEnqueued;
    CMinWait<WAIT_TIME> mWait;
    bool mPinReady;
};

#endif

/// @brief Driver traits for `SlimBridgeController` (one driver per
/// pin/timing/wait/XTRA0 specialization; never shared across timings).
template <int DATA_PIN, typename TIMING, int WAIT_TIME, int XTRA0>
struct ClocklessStm32Traits {
    using Driver = ClocklessStm32Driver<DATA_PIN, TIMING, WAIT_TIME, XTRA0>;

    /// Storage for this specialization's driver: a static member (no
    /// function-local static guard), handed out as a no-tracking shared_ptr.
    static Driver& driver() FL_NO_EXCEPT;

    static fl::shared_ptr<Driver> instancePtr() FL_NO_EXCEPT;
    static IChannelDriver& instance() FL_NO_EXCEPT;

    /// Idempotent: the driver name is unique per specialization, so this only
    /// skips re-registering this exact driver.
    static void registerWithManager() FL_NO_EXCEPT;
};

/// @brief STM32 Clockless LED Controller (slim bridge, issue #4594)
/// @tparam DATA_PIN Pin number for data line output
/// @tparam TIMING ChipsetTiming structure containing T1, T2, T3, and RESET values
/// @tparam RGB_ORDER Color order (RGB, GRB, etc.)
/// @tparam XTRA0 Extra trailing zero bits sent after each byte
/// @tparam FLIP Flip the output bit order if true (unused)
/// @tparam WAIT_TIME Wait time between updates in microseconds
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
class ClocklessController
    : public SlimBridgeController<DATA_PIN, TIMING, RGB_ORDER, WAIT_TIME,
                                  ClocklessStm32Traits<DATA_PIN, TIMING, WAIT_TIME, XTRA0>, XTRA0> {
public:
    u16 getMaxRefreshRate() const FL_NO_EXCEPT override;
};

}  // namespace fl

FL_DISABLE_WARNING_POP
