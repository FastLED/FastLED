#define FASTLED_INTERNAL
#include <stdint.h>  // for u8, u32, u16
#include "FastLED.h"
#include "fl/engine_events.h"
#include "fl/compiler_control.h"
#include "fl/channels/channel.h"
#include "fl/channels/channel_events.h"
#include "fl/channels/bus_manager.h"
#include "fl/trace.h"
#include "fl/channels/engine.h"  // for IChannelEngine
#include "fl/delay.h"  // for delayMicroseconds
#include "fl/stl/assert.h"  // for FL_ASSERT
#include "hsv2rgb.h"  // for CRGB
#include "fl/int.h"  // for u32, u16
#include "platforms/init.h"  // IWYU pragma: keep
#include "fl/channels/config.h"  // for ChannelConfig
#include "platforms/is_platform.h"

/// @file FastLED.cpp
/// Central source file for FastLED, implements the CFastLED class/object

#ifndef MAX_CLED_CONTROLLERS
#ifdef FL_IS_AVR
// if mega or leonardo, allow more controllers
#if defined(FL_IS_AVR_ATMEGA_2560) || defined(FL_IS_AVR_ATMEGA)
#define MAX_CLED_CONTROLLERS 16
#else
#define MAX_CLED_CONTROLLERS 8
#endif
#else
#define MAX_CLED_CONTROLLERS 64
#endif  // FL_IS_AVR
#endif  // MAX_CLED_CONTROLLERS

#if defined(FL_IS_SAM)
volatile fl::u32 fuckit;
#endif


#ifndef FASTLED_NO_ATEXIT
#define FASTLED_NO_ATEXIT 0
#endif

#if !FASTLED_NO_ATEXIT
extern "C" FL_LINK_WEAK int atexit(void (* /*func*/ )()) { return 0; }
#endif

#ifdef FASTLED_NEEDS_YIELD
extern "C" void yield(void) { }
#endif

// Implementation of cled_contoller_size() moved to src/fl/fastled_internal.cpp

fl::u8 get_brightness();
fl::u16 get_fps();

/// Pointer to the matrix object when using the Smart Matrix Library
/// @see https://github.com/pixelmatix/SmartMatrix
void *pSmartMatrix = nullptr;

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_GLOBAL_CONSTRUCTORS

CFastLED FastLED;  // global constructor allowed in this case.

FL_DISABLE_WARNING_POP

CLEDController *CLEDController::m_pHead = nullptr;
CLEDController *CLEDController::m_pTail = nullptr;
static fl::u32 lastshow = 0;

/// Global frame counter, used for debugging ESP implementations
/// @todo Include in FASTLED_DEBUG_COUNT_FRAME_RETRIES block?
fl::u32 _frame_cnt=0;

/// Global frame retry counter, used for debugging ESP implementations
/// @todo Include in FASTLED_DEBUG_COUNT_FRAME_RETRIES block?
fl::u32 _retry_cnt=0;

// uint32_t CRGB::Squant = ((uint32_t)((__TIME__[4]-'0') * 28))<<16 | ((__TIME__[6]-'0')*50)<<8 | ((__TIME__[7]-'0')*28);

CFastLED::CFastLED() {
	// clear out the array of led controllers
	// m_nControllers = 0;
	m_Scale = 255;
	m_nFPS = 0;
	m_pPowerFunc = nullptr;
	m_nPowerData = 0xFFFFFFFF;
	m_nMinMicros = 0;
}

void CFastLED::init() {
	// Call platform-specific initialization once
	// Uses the trampoline pattern: platforms/init.h dispatches to platform-specific headers
	// FL_RUN_ONCE ensures this is only called once, even if init() is called multiple times
	FL_RUN_ONCE(fl::platforms::init());
}

int CFastLED::size() {
	return (*this)[0].size();
}

CRGB* CFastLED::leds() {
	return (*this)[0].leds();
}

CLEDController &CFastLED::addLeds(CLEDController *pLed,
								  CRGB *data,
								  int nLedsOrOffset, int nLedsIfOffset) {
	int nOffset = (nLedsIfOffset > 0) ? nLedsOrOffset : 0;
	int nLeds = (nLedsIfOffset > 0) ? nLedsIfOffset : nLedsOrOffset;

	pLed->init();
	pLed->setLeds(data + nOffset, nLeds);
	FastLED.setMaxRefreshRate(pLed->getMaxRefreshRate(),true);
	fl::EngineEvents::onStripAdded(pLed, nLedsOrOffset - nOffset);
	return *pLed;
}

void CFastLED::add(fl::ChannelPtr channel) {
	if (!channel) {
		return;
	}

	// Register with ChannelBusManager for top-down encoding
	// All channels use ChannelBusManager (no direct engine binding anymore)
	// Note: ChannelBusManager::registerChannel() fires onChannelAdded event
	fl::ChannelBusManager::instance().registerChannel(channel);
}

void CFastLED::remove(fl::ChannelPtr channel) {
	if (!channel) {
		return;
	}

	// Unregister from ChannelBusManager
	// Note: ChannelBusManager::unregisterChannel() fires onChannelRemoved event
	fl::ChannelBusManager::instance().unregisterChannel(channel);
}

void CFastLED::reset(ResetFlags flags) {
	// Lambda to check if flag is set, clear it, and return true if it was set
	auto clearFlag = [&flags](ResetFlags flag) -> bool {
		if ((flags & flag) != ResetFlags::NONE) {
			flags = static_cast<ResetFlags>(static_cast<fl::u32>(flags) & ~static_cast<fl::u32>(flag));
			return true;
		}
		return false;
	};


	// Reset POWER_SETTINGS - reset power management to defaults
	if (clearFlag(ResetFlags::POWER_SETTINGS)) {
		FastLED.m_pPowerFunc = nullptr;      // No power limiting function
		FastLED.m_nPowerData = 0xFFFFFFFF;   // No power limit (max value)
	}

	// Reset BRIGHTNESS - reset global brightness to 255 (full brightness)
	if (clearFlag(ResetFlags::BRIGHTNESS)) {
		FastLED.m_Scale = 255;
	}

	// Reset REFRESH_RATE - reset refresh rate limiting to unlimited
	if (clearFlag(ResetFlags::REFRESH_RATE)) {
		FastLED.m_nMinMicros = 0;  // No minimum delay between frames
	}

	// Reset FPS_COUNTER - reset FPS tracking counter to 0
	if (clearFlag(ResetFlags::FPS_COUNTER)) {
		FastLED.m_nFPS = 0;
	}

	// Reset CHANNELS - remove all channels AND legacy controllers from controller list
	if (clearFlag(ResetFlags::CHANNELS)) {
		// Always wait for all channel bus transmissions to complete first
		FastLED.wait();

		// Get channel list from ChannelBusManager and remove all channels
		fl::ChannelBusManager& manager = fl::channelBusManager();

		// Make a copy of the channel list because unregisterChannel() modifies it
		fl::vector<fl::ChannelPtr> channelsCopy = manager.getChannels();
		for (auto& channel : channelsCopy) {
			remove(channel);
		}

		// Reset bus manager state (clear enqueued and transmitting channels)
		manager.reset();

		// Also remove ALL legacy CLEDControllers from the linked list
		// This handles controllers created with FastLED.addLeds<>() which are not
		// registered as Channel objects in ChannelBusManager
		while (CLEDController* head = CLEDController::head()) {
			head->removeFromDrawList();
		}
	}

	// Reset CHANNEL_ENGINES - clear all registered channel engines
	if (clearFlag(ResetFlags::CHANNEL_ENGINES)) {
		// Always wait for all channel bus transmissions to complete first
		FastLED.wait();
		fl::ChannelBusManager& manager = fl::channelBusManager();
		manager.clearAllEngines();
	}

	// Ensure all flags were handled - catches typos or missing implementations
	FL_ASSERT(flags == ResetFlags::NONE, "Unhandled flags in FastLED.reset()");
}

// This is bad code. But it produces the smallest binaries for reasons
// beyond mortal comprehensions.
// Instead of iterating through the link list for onBeginFrame(), beginShowLeds()
// and endShowLeds(): store the pointers in an array and iterate through that.
//
// static uninitialized gControllersData produces the smallest binary on attiny85.
static void* gControllersData[MAX_CLED_CONTROLLERS];

FL_KEEP_ALIVE void CFastLED::show(fl::u8 scale) {
	FL_SCOPED_TRACE;
	onBeginFrame();
	while(m_nMinMicros && ((fl::micros()-lastshow) < m_nMinMicros));
	lastshow = fl::micros();

	// If we have a function for computing power, use it!
	if(m_pPowerFunc) {
		scale = (*m_pPowerFunc)(scale, m_nPowerData);
	}


	int length = 0;
	CLEDController *pCur = CLEDController::head();

	while(pCur && length < MAX_CLED_CONTROLLERS) {
		if (pCur->getEnabled()) {
			gControllersData[length] = pCur->beginShowLeds(pCur->size());
		} else {
			gControllersData[length] = nullptr;
		}
		length++;
		// Only disable dither if FPS is actively being tracked (fps > 0)
		// FPS = 0 means FPS tracking hasn't started, so respect user's dither setting
		if (m_nFPS > 0 && m_nFPS < 100) { pCur->setDither(0); }
		pCur = pCur->next();
	}

	pCur = CLEDController::head();
	for (length = 0; length < MAX_CLED_CONTROLLERS && pCur; length++) {
		if (pCur->getEnabled()) {
			pCur->showLedsInternal(scale);
		}
		pCur = pCur->next();

	}

	length = 0;  // Reset length to 0 and iterate again.
	pCur = CLEDController::head();
	while(pCur && length < MAX_CLED_CONTROLLERS) {
		if (pCur->getEnabled()) {
			pCur->endShowLeds(gControllersData[length]);
		}
		length++;
		pCur = pCur->next();
	}
	countFPS();
	onEndFrame();
	onEndShowLeds();
}

void CFastLED::onEndFrame() {
	#if FASTLED_HAS_ENGINE_EVENTS
	fl::EngineEvents::onEndFrame();
	#endif
}

int CFastLED::count() {
    int x = 0;
	CLEDController *pCur = CLEDController::head();
	while( pCur) {
        ++x;
		pCur = pCur->next();
	}
    return x;
}

CLEDController & CFastLED::operator[](int x) {
	CLEDController *pCur = CLEDController::head();
	while(x-- && pCur) {
		pCur = pCur->next();
	}
	if(pCur == nullptr) {
		return *(CLEDController::head());
	} else {
		return *pCur;
	}
}

void CFastLED::showColor(const CRGB & color, fl::u8 scale) {
	while(m_nMinMicros && ((fl::micros()-lastshow) < m_nMinMicros));
	lastshow = fl::micros();

	// If we have a function for computing power, use it!
	if(m_pPowerFunc) {
		scale = (*m_pPowerFunc)(scale, m_nPowerData);
	}

	int length = 0;
	CLEDController *pCur = CLEDController::head();
	while(pCur && length < MAX_CLED_CONTROLLERS) {
		if (pCur->getEnabled()) {
			gControllersData[length] = pCur->beginShowLeds(pCur->size());
		} else {
			gControllersData[length] = nullptr;
		}
		length++;
		pCur = pCur->next();
	}

	pCur = CLEDController::head();
	while(pCur && length < MAX_CLED_CONTROLLERS) {
		// Only disable dither if FPS is actively being tracked (fps > 0)
		// FPS = 0 means FPS tracking hasn't started, so respect user's dither setting
		if(m_nFPS > 0 && m_nFPS < 100) { pCur->setDither(0); }
		if (pCur->getEnabled()) {
			pCur->showColorInternal(color, scale);
		}
		pCur = pCur->next();
	}

	pCur = CLEDController::head();
	length = 0;  // Reset length to 0 and iterate again.
	while(pCur && length < MAX_CLED_CONTROLLERS) {
		if (pCur->getEnabled()) {
			pCur->endShowLeds(gControllersData[length]);
		}
		length++;
		pCur = pCur->next();
	}
	countFPS();
	onEndFrame();
}

void CFastLED::clear(bool writeData) {
	if(writeData) {
		showColor(CRGB(0,0,0), 0);
	}
    clearData();
}

void CFastLED::clearData() {
	CLEDController *pCur = CLEDController::head();
	while(pCur) {
		pCur->clearLedDataInternal();
		pCur = pCur->next();
	}
}

void CFastLED::delay(unsigned long ms) {
	unsigned long start = fl::millis();
        do {
#ifndef FASTLED_ACCURATE_CLOCK
		// make sure to allow at least one ms to pass to ensure the clock moves
		// forward
		fl::delay(1);
#endif
		show();
		yield();
	}
	while((fl::millis()-start) < ms);
}

void CFastLED::setTemperature(const CRGB & temp) {
	CLEDController *pCur = CLEDController::head();
	while(pCur) {
		pCur->setTemperature(temp);
		pCur = pCur->next();
	}
}

void CFastLED::setCorrection(const CRGB & correction) {
	CLEDController *pCur = CLEDController::head();
	while(pCur) {
		pCur->setCorrection(correction);
		pCur = pCur->next();
	}
}

void CFastLED::setDither(fl::u8 ditherMode)  {
	CLEDController *pCur = CLEDController::head();
	while(pCur) {
		pCur->setDither(ditherMode);
		pCur = pCur->next();
	}
}

fl::u32 CFastLED::getEstimatedPowerInMilliWatts(bool apply_limiter) const {
	fl::u32 total_power_mW = 0;

	// Sum unscaled power from all LED controllers using visitor pattern
	CLEDController::visitControllers([&total_power_mW](const CLEDController* /*controller*/, fl::span<const CRGB> leds) {
		if (!leds.empty()) {
			total_power_mW += calculate_unscaled_power_mW(leds);
		}
	});

	// Determine effective brightness
	fl::u8 effective_brightness = m_Scale;

	if (apply_limiter && m_pPowerFunc) {
		// Power limiting is enabled and user wants limited power - calculate brightness after limiting
		// This calls calculate_max_brightness_for_power_mW(m_Scale, m_nPowerData)
		effective_brightness = (*m_pPowerFunc)(m_Scale, m_nPowerData);
	}

	// Scale by effective brightness (linear scaling)
	// Power = unscaled_power * (brightness / 255)
	// Uses scale32by8() to prevent overflow and ensure proper rounding
	// Note: MCU power consumption is NOT included - caller should add platform-specific MCU power if needed
	return fl::scale32by8(total_power_mW, effective_brightness);
}

//
// template<int m, int n> void transpose8(unsigned char A[8], unsigned char B[8]) {
// 	uint32_t x, y, t;
//
// 	// Load the array and pack it into x and y.
//   	y = *(unsigned int*)(A);
// 	x = *(unsigned int*)(A+4);
//
// 	// x = (A[0]<<24)   | (A[m]<<16)   | (A[2*m]<<8) | A[3*m];
// 	// y = (A[4*m]<<24) | (A[5*m]<<16) | (A[6*m]<<8) | A[7*m];
//
        // // pre-transform x
        // t = (x ^ (x >> 7)) & 0x00AA00AA;  x = x ^ t ^ (t << 7);
        // t = (x ^ (x >>14)) & 0x0000CCCC;  x = x ^ t ^ (t <<14);
				//
        // // pre-transform y
        // t = (y ^ (y >> 7)) & 0x00AA00AA;  y = y ^ t ^ (t << 7);
        // t = (y ^ (y >>14)) & 0x0000CCCC;  y = y ^ t ^ (t <<14);
				//
        // // final transform
        // t = (x & 0xF0F0F0F0) | ((y >> 4) & 0x0F0F0F0F);
        // y = ((x << 4) & 0xF0F0F0F0) | (y & 0x0F0F0F0F);
        // x = t;
//
// 	B[7*n] = y; y >>= 8;
// 	B[6*n] = y; y >>= 8;
// 	B[5*n] = y; y >>= 8;
// 	B[4*n] = y;
//
//   B[3*n] = x; x >>= 8;
// 	B[2*n] = x; x >>= 8;
// 	B[n] = x; x >>= 8;
// 	B[0] = x;
// 	// B[0]=x>>24;    B[n]=x>>16;    B[2*n]=x>>8;  B[3*n]=x>>0;
// 	// B[4*n]=y>>24;  B[5*n]=y>>16;  B[6*n]=y>>8;  B[7*n]=y>>0;
// }
//
// void transposeLines(Lines & out, Lines & in) {
// 	transpose8<1,2>(in.bytes, out.bytes);
// 	transpose8<1,2>(in.bytes + 8, out.bytes + 1);
// }


/// Unused value
/// @todo Remove?
extern int noise_min;

/// Unused value
/// @todo Remove?
extern int noise_max;

void CFastLED::countFPS(int nFrames) {
	static int br = 0;
	static fl::u32 lastframe = 0; // fl::millis();

	if(br++ >= nFrames) {
		fl::u32 now = fl::millis();
		now -= lastframe;
		if(now == 0) {
			now = 1; // prevent division by zero below
		}
		m_nFPS = (br * 1000) / now;
		br = 0;
		lastframe = fl::millis();
	}
}

void CFastLED::setMaxRefreshRate(fl::u16 refresh, bool constrain) {
	if(constrain) {
		// if we're constraining, the new value of m_nMinMicros _must_ be higher than previously (because we're only
		// allowed to slow things down if constraining)
		if(refresh > 0) {
			m_nMinMicros = ((1000000 / refresh) > m_nMinMicros) ? (1000000 / refresh) : m_nMinMicros;
		}
	} else if(refresh > 0) {
		m_nMinMicros = 1000000 / refresh;
	} else {
		m_nMinMicros = 0;
	}
}


fl::u8 get_brightness() {
	return FastLED.getBrightness();
}

fl::u16 get_fps() {
	return FastLED.getFPS();
}

// ============================================================================
// Deprecated Power Management Wrapper Functions
// ============================================================================
// These functions wrap the CFastLED singleton methods for backward compatibility.
// They were originally in power_mgt.cpp but have been moved here to avoid
// circular dependencies (power_mgt.cpp should not include FastLED.h).

void set_max_power_in_volts_and_milliamps(fl::u8 volts, fl::u32 milliamps)
{
	FastLED.setMaxPowerInVoltsAndMilliamps(volts, milliamps);
}

void set_max_power_in_milliwatts(fl::u32 powerInmW)
{
	FastLED.setMaxPowerInMilliWatts(powerInmW);
}

void show_at_max_brightness_for_power()
{
	// power management usage is now in FastLED.show, no need for this function
	FastLED.show();
}

void delay_at_max_brightness_for_power(fl::u16 ms)
{
	FastLED.delay(ms);
}

// ============================================================================
// Channel Bus Manager Controls
// ============================================================================

#ifdef FL_IS_ESP32
void CFastLED::setDriverEnabled(const char* name, bool enabled) {
	fl::ChannelBusManager& manager = fl::channelBusManager();
	manager.setDriverEnabled(name, enabled);
}

bool CFastLED::setExclusiveDriver(const char* name) {
	fl::ChannelBusManager& manager = fl::channelBusManager();
	return manager.setExclusiveDriver(name);
}

bool CFastLED::isDriverEnabled(const char* name) const {
	fl::ChannelBusManager& manager = fl::channelBusManager();
	return manager.isDriverEnabled(name);
}

fl::size CFastLED::getDriverCount() const {
	fl::ChannelBusManager& manager = fl::channelBusManager();
	return manager.getDriverCount();
}

fl::span<const fl::DriverInfo> CFastLED::getDriverInfos() const {
	fl::ChannelBusManager& manager = fl::channelBusManager();
	return manager.getDriverInfos();
}
#endif

// ============================================================================
// Wait for channel bus transmissions
// ============================================================================

void CFastLED::wait() {
	fl::ChannelBusManager& manager = fl::channelBusManager();
	// Wait for all engines to become READY
	// Calls async_run() and uses time-based delays to avoid busy-waiting
	manager.wait();
}

// ============================================================================
// Runtime Channel API Implementation
// ============================================================================

fl::ChannelPtr CFastLED::add(const fl::ChannelConfig& config) {
    fl::ChannelBusManager& manager = fl::channelBusManager();
    FL_ASSERT(manager.getDriverCount() > 0,
              "No channel drivers available - channel API requires at least one registered driver");
    auto channel = fl::Channel::create(config);
    add(channel);
    return channel;
}

fl::vector<fl::ChannelPtr> CFastLED::add(fl::span<const fl::ChannelConfig> configs) {
    fl::vector<fl::ChannelPtr> channels;
    channels.reserve(configs.size());

    for (const auto& config : configs) {
        channels.push_back(add(config));
    }

    return channels;
}

fl::vector<fl::ChannelPtr> CFastLED::add(fl::initializer_list<fl::ChannelConfig> configs) {
    fl::vector<fl::ChannelPtr> channels;
    channels.reserve(configs.size());

    for (const auto& config : configs) {
        channels.push_back(add(config));
    }

    return channels;
}

fl::vector<fl::ChannelPtr> CFastLED::add(const fl::MultiChannelConfig& multiConfig) {
    fl::vector<fl::ChannelPtr> channels;
    channels.reserve(multiConfig.mChannels.size());

    for (const auto& configPtr : multiConfig.mChannels) {
        if (configPtr) {
            channels.push_back(add(*configPtr));
        }
    }

    return channels;
}

// ============================================================================

#ifdef NEED_CXX_BITS
namespace __cxxabiv1
{
	#if !defined(FL_IS_ESP8266) && !defined(FL_IS_ESP32)
	extern "C" void __cxa_pure_virtual (void) {}
	#endif

	/* guard variables */

	/* The ABI requires a 64-bit type.  */
	__extension__ typedef int __guard __attribute__((mode(__DI__)));

	extern "C" int __cxa_guard_acquire (__guard *) FL_LINK_WEAK;
	extern "C" void __cxa_guard_release (__guard *) FL_LINK_WEAK;
	extern "C" void __cxa_guard_abort (__guard *) FL_LINK_WEAK;

	extern "C" int __cxa_guard_acquire (__guard *g)
	{
		return !*(char *)(g);
	}

	extern "C" void __cxa_guard_release (__guard *g)
	{
		*(char *)g = 1;
	}

	extern "C" void __cxa_guard_abort (__guard *)
	{

	}
}
#endif


void CFastLED::onBeginFrame() {
	#if FASTLED_HAS_ENGINE_EVENTS
	fl::EngineEvents::onBeginFrame();
	#endif
}


void CFastLED::onEndShowLeds() {
	#if FASTLED_HAS_ENGINE_EVENTS
	fl::EngineEvents::onEndShowLeds();
	#endif
}

fl::ChannelEvents& CFastLED::channelEvents() {
	return fl::ChannelEvents::instance();
}
