// IWYU pragma: private

#ifndef __INC_CLOCKLESS_ARM_D51
#define __INC_CLOCKLESS_ARM_D51

/// @file clockless_arm_d51.h
/// @brief SAMD51 legacy clockless controller routed through the slim bridge.
///
/// `ClocklessController` is a `fl::SlimBridgeController` (issue #4593): the
/// legacy `addLeds<>()` path encodes pixels into a `ChannelData` buffer and
/// hands it to a per-pin `ClocklessSamd51Driver`. That driver keeps the
/// DWT-cycle-counter bit loop that previously lived in this controller as
/// the byte-emitting engine (the shared `BitBangChannelDriver`'s
/// `delayNanoseconds` phases are too coarse for WS281x timing here).
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
#include "eorder.h"
#include "fastled_delay.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER

namespace fl {
#define ARM_DEMCR               (*(volatile u32 *)0xE000EDFC) // Debug Exception and Monitor Control
#define ARM_DEMCR_TRCENA                (1 << 24)        // Enable debugging & monitoring blocks
#define ARM_DWT_CTRL            (*(volatile u32 *)0xE0001000) // DWT control register
#define ARM_DWT_CTRL_CYCCNTENA          (1 << 0)                // Enable cycle count
#define ARM_DWT_CYCCNT          (*(volatile u32 *)0xE0001004) // Cycle count register


#define FL_CLOCKLESS_CONTROLLER_DEFINED 1

/// @brief Blocking single-pin clockless driver for SAMD51 (DWT-timed loop).
///
/// The input buffer is already colour-ordered, scaled and dithered by the
/// bridge's `PixelIterator`; this driver only shifts the bytes out.
template <int DATA_PIN, typename TIMING, int XTRA0, int WAIT_TIME>
class ClocklessSamd51Driver : public IChannelDriver {
	// Extract timing values from struct and convert from nanoseconds to clock cycles
	// Formula: cycles = (nanoseconds * CPU_MHz + 500) / 1000
	// The +500 provides rounding to nearest integer
	static constexpr u32 T1 = (TIMING::T1 * (F_CPU / 1000000UL) + 500) / 1000;
	static constexpr u32 T2 = (TIMING::T2 * (F_CPU / 1000000UL) + 500) / 1000;
	static constexpr u32 T3 = (TIMING::T3 * (F_CPU / 1000000UL) + 500) / 1000;
	typedef typename FastPin<DATA_PIN>::port_ptr_t data_ptr_t;
	typedef typename FastPin<DATA_PIN>::port_t data_t;

public:
	ClocklessSamd51Driver() FL_NO_EXCEPT : mPinReady(false) {}

	bool canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT override {
		return data && data->isClockless() && data->getPin() == DATA_PIN;
	}

	void enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT override {
		if (channelData) {
			mEnqueued.push_back(fl::move(channelData));
		}
	}

	void show() FL_NO_EXCEPT override {
		if (mEnqueued.empty()) {
			return;
		}
		if (!mPinReady) {
			FastPin<DATA_PIN>::setOutput();
			mPinReady = true;
		}
		for (fl::size i = 0; i < mEnqueued.size(); ++i) {
			const ChannelDataPtr& ch = mEnqueued[i];
			if (!ch) {
				continue;
			}
			ch->setInUse(true);
			const fl::vector_psram<u8>& bytes = ch->getData();
			mWait.wait();
			if (!sendBytes(bytes.data(), static_cast<u32>(bytes.size()))) {
				sei(); delayMicroseconds(WAIT_TIME); cli();
				sendBytes(bytes.data(), static_cast<u32>(bytes.size()));
			}
			mWait.mark();
			ch->setInUse(false);
		}
		mEnqueued.clear();
	}

	DriverState poll() FL_NO_EXCEPT override {
		return DriverState(DriverState::READY);
	}

	fl::string getName() const FL_NO_EXCEPT override {
		fl::string name = fl::string::from_literal("SAMD51_CLOCKLESS_P");
		name.append(static_cast<i32>(DATA_PIN));
		return name;
	}

	Capabilities getCapabilities() const FL_NO_EXCEPT override {
		return Capabilities(true, false);
	}

private:
	template<int BITS> __attribute__ ((always_inline)) inline static void writeBits(FASTLED_REGISTER u32 & next_mark, FASTLED_REGISTER data_ptr_t port, FASTLED_REGISTER data_t hi, FASTLED_REGISTER data_t lo, FASTLED_REGISTER u8 & b) FL_NO_EXCEPT {
		for(FASTLED_REGISTER u32 i = BITS-1; i > 0; --i) {
			while(ARM_DWT_CYCCNT < next_mark);
			next_mark = ARM_DWT_CYCCNT + (T1+T2+T3);
			FastPin<DATA_PIN>::fastset(port, hi);
			if(b&0x80) {
				while((next_mark - ARM_DWT_CYCCNT) > (T3+(2*(F_CPU/24000000))));
				FastPin<DATA_PIN>::fastset(port, lo);
			} else {
				while((next_mark - ARM_DWT_CYCCNT) > (T2+T3+(2*(F_CPU/24000000))));
				FastPin<DATA_PIN>::fastset(port, lo);
			}
			b <<= 1;
		}

		while(ARM_DWT_CYCCNT < next_mark);
		next_mark = ARM_DWT_CYCCNT + (T1+T2+T3);
		FastPin<DATA_PIN>::fastset(port, hi);

		if(b&0x80) {
			while((next_mark - ARM_DWT_CYCCNT) > (T3+(2*(F_CPU/24000000))));
			FastPin<DATA_PIN>::fastset(port, lo);
		} else {
			while((next_mark - ARM_DWT_CYCCNT) > (T2+T3+(2*(F_CPU/24000000))));
			FastPin<DATA_PIN>::fastset(port, lo);
		}
	}

	/// Emit `len` raw bytes.
	/// @return 0 if an interrupt overran the frame, nonzero on success.
	static u32 sendBytes(const u8* bytes, u32 len) FL_NO_EXCEPT {
		// Get access to the clock
		ARM_DEMCR    |= ARM_DEMCR_TRCENA;
		ARM_DWT_CTRL |= ARM_DWT_CTRL_CYCCNTENA;
		ARM_DWT_CYCCNT = 0;

		FASTLED_REGISTER data_ptr_t port = FastPin<DATA_PIN>::port();
		FASTLED_REGISTER data_t hi = FastPin<DATA_PIN>::hival();
		FASTLED_REGISTER data_t lo = FastPin<DATA_PIN>::loval();
		*port = lo;

		cli();
		u32 next_mark = ARM_DWT_CYCCNT + (T1+T2+T3);

		u32 i = 0;
		while (i < len) {
			#if (FASTLED_ALLOW_INTERRUPTS == 1)
			cli();
			// if interrupts took longer than 45µs, punt on the current frame
			if(ARM_DWT_CYCCNT > next_mark) {
				if((ARM_DWT_CYCCNT-next_mark) > ((WAIT_TIME-INTERRUPT_THRESHOLD)*CLKS_PER_US)) { sei(); return 0; }
			}

			hi = FastPin<DATA_PIN>::hival();
			lo = FastPin<DATA_PIN>::loval();
			#endif
			// Emit one pixel's worth (up to 3 bytes) with interrupts held off,
			// matching the historical per-pixel interrupt window.
			u32 end = i + 3;
			if (end > len) {
				end = len;
			}
			for (; i < end; ++i) {
				FASTLED_REGISTER u8 b = bytes[i];
				writeBits<8+XTRA0>(next_mark, port, hi, lo, b);
			}
			#if (FASTLED_ALLOW_INTERRUPTS == 1)
			sei();
			#endif
		}

		sei();
		return ARM_DWT_CYCCNT | 1u;
	}

	fl::vector<ChannelDataPtr> mEnqueued;
	CMinWait<WAIT_TIME> mWait;
	bool mPinReady;
};

/// @brief Driver traits for `SlimBridgeController` (per pin/timing singleton).
template <int DATA_PIN, typename TIMING, int XTRA0, int WAIT_TIME>
struct ClocklessSamd51Traits {
	using Driver = ClocklessSamd51Driver<DATA_PIN, TIMING, XTRA0, WAIT_TIME>;

	/// Storage for this pin/timing's driver: a static member (no function-local
	/// static guard), handed out as a no-tracking shared_ptr.
	static Driver sDriver;

	static fl::shared_ptr<Driver> instancePtr() FL_NO_EXCEPT {
		return fl::make_shared_no_tracking(sDriver);
	}

	/// The driver actually used for this pin: the first one registered under
	/// the pin-only name, so every timing specialization on the same pin queues
	/// frames to the single registered driver.
	static IChannelDriver& instance() FL_NO_EXCEPT {
		fl::shared_ptr<IChannelDriver> existing =
			ChannelManager::registry().findDriverByName(sDriver.getName());
		if (existing) {
			return *existing;
		}
		return sDriver;
	}

	/// Idempotent: skip if a driver for this pin is already registered (by any
	/// timing specialization), so a second controller does not replace it.
	static void registerWithManager() FL_NO_EXCEPT {
		ChannelManager& manager = ChannelManager::registry();
		if (manager.findDriverByName(sDriver.getName())) {
			return;
		}
		manager.addDriver(0, instancePtr());
	}
};

template <int DATA_PIN, typename TIMING, int XTRA0, int WAIT_TIME>
typename ClocklessSamd51Traits<DATA_PIN, TIMING, XTRA0, WAIT_TIME>::Driver ClocklessSamd51Traits<DATA_PIN, TIMING, XTRA0, WAIT_TIME>::sDriver;

/// @brief ARM D51 (SAMD51) Clockless LED Controller
/// @tparam DATA_PIN Pin number for data line output
/// @tparam TIMING ChipsetTiming structure containing T1, T2, T3, and RESET values
/// @tparam RGB_ORDER Color order (RGB, GRB, etc.)
/// @tparam XTRA0 Extra zero bits emitted per byte
/// @tparam FLIP Flip the output bit order if true (unused)
/// @tparam WAIT_TIME Wait time between updates in microseconds
///
/// Example usage with named timing constant:
/// @code
///   ClocklessController<5, TIMING_WS2812_800KHZ, GRB> controller;
/// @endcode
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
class ClocklessController
	: public SlimBridgeController<DATA_PIN, TIMING, RGB_ORDER, WAIT_TIME,
	                              ClocklessSamd51Traits<DATA_PIN, TIMING, XTRA0, WAIT_TIME>, XTRA0> {
public:
	u16 getMaxRefreshRate() const FL_NO_EXCEPT override { return 400; }
};
}  // namespace fl

FL_DISABLE_WARNING_POP

#endif
