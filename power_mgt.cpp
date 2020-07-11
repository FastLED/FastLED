#define FASTLED_INTERNAL
#include "FastLED.h"
#include "power_mgt.h"
#include <array>

FASTLED_NAMESPACE_BEGIN

//// POWER MANAGEMENT

// These power usage values are approximate, and your exact readings
// will be slightly (10%?) different from these.
//
// They were arrived at by actually measuing the power draw of a number
// of different LED strips, and a bunch of closed-loop-feedback testing
// to make sure that if we USE these values, we stay at or under
// the target power consumption.
// Actual power consumption is much, much more complicated and has
// to include things like voltage drop, etc., etc.
// However, this is good enough for most cases, and almost certainly better
// than no power management at all.
//
// You're welcome to adjust these values as needed; there may eventually be an API
// for changing these on the fly, but it saves codespace and RAM to have them
// be compile-time constants.

constexpr float POWER_EXPONENENT = 0.87; // 50% pwm will result in 0.5^0.87 = 54.7% of power usage
constexpr uint8_t gRed_mW   = 16 * 5; // 16mA @ 5v = 80mW
constexpr uint8_t gGreen_mW = 11 * 5; // 11mA @ 5v = 55mW
constexpr uint8_t gBlue_mW  = 15 * 5; // 15mA @ 5v = 75mW
constexpr uint8_t gDark_mW  = 1 * 5;  // 1mA @ 5v = 75mW

// Alternate measurements for sk6805-1515
// constexpr float POWER_EXPONENENT = 0.87;
// constexpr uint8_t gRed_mW   = 25;
// constexpr uint8_t gGreen_mW = 18;
// constexpr uint8_t gBlue_mW  = 24;
// constexpr uint8_t gDark_mW  = 4;

// Alternate calibration by RAtkins via pre-PSU wattage measurments;
// these are all probably about 20%-25% too high due to PSU heat losses,
// but if you're measuring wattage on the PSU input side, this may
// be a better set of calibrations.  (WS2812B)
//  constexpr uint8_t gRed_mW   = 100;
//  constexpr uint8_t gGreen_mW =  48;
//  constexpr uint8_t gBlue_mW  = 100;
//  constexpr uint8_t gDark_mW  =  12;



#define POWER_LED 0
#define POWER_DEBUG_PRINT 0


// Power consumed by the MCU
constexpr uint8_t gMCU_mW  =  25 * 5; // 25mA @ 5v = 125 mW

static uint8_t gMaxPowerIndicatorLEDPinNumber = 0; // default = Arduino onboard LED pin.  set to zero to skip this.


// if defined, use linear power scaling. Cheapest but inaccurate (~20%).
#ifdef FASTLED_POWER_SCALING_FAST
constexpr uint8_t getPowerValue(uint8_t brightness) {
    return brightness;
}
constexpr uint8_t getReversePowerValue(uint8_t brightness_scaled) {
    return brightness_scaled;
}
#else
#define calcPowerValue(brightness) pow(brightness, POWER_EXPONENENT) * pow(256, 1 - POWER_EXPONENENT)
constexpr uint8_t getReversePowerValue(uint8_t brightness_scaled) {
    return pow(brightness_scaled / pow(256, 1 - POWER_EXPONENENT), 1.f/POWER_EXPONENENT);
}
// if defined, compute power value each time.
#ifdef FASTLED_POWER_SCALING_COMPUTE
constexpr uint8_t getPowerValue(uint8_t brightness) {
    return calcPowerValue(brightness);
}
#else
// precompute values and store them in an array.

#if __cplusplus >= 201700L
// c++17
constexpr std::array<uint8_t, 256> POWERTABLE = [] {
  std::array<uint8_t, 256> A = {};
  for (unsigned i = 0; i < 256; i++) {
    A[i] = calcPowerValue(i);
  }
  return A;
}();
#else
constexpr std::array<uint8_t, 256> POWERTABLE = {
    calcPowerValue(0), calcPowerValue(1), calcPowerValue(2), calcPowerValue(3), calcPowerValue(4), calcPowerValue(5), calcPowerValue(6), calcPowerValue(7), 
    calcPowerValue(8), calcPowerValue(9), calcPowerValue(10), calcPowerValue(11), calcPowerValue(12), calcPowerValue(13), calcPowerValue(14), calcPowerValue(15), 
    calcPowerValue(16), calcPowerValue(17), calcPowerValue(18), calcPowerValue(19), calcPowerValue(20), calcPowerValue(21), calcPowerValue(22), calcPowerValue(23), 
    calcPowerValue(24), calcPowerValue(25), calcPowerValue(26), calcPowerValue(27), calcPowerValue(28), calcPowerValue(29), calcPowerValue(30), calcPowerValue(31), 
    calcPowerValue(32), calcPowerValue(33), calcPowerValue(34), calcPowerValue(35), calcPowerValue(36), calcPowerValue(37), calcPowerValue(38), calcPowerValue(39), 
    calcPowerValue(40), calcPowerValue(41), calcPowerValue(42), calcPowerValue(43), calcPowerValue(44), calcPowerValue(45), calcPowerValue(46), calcPowerValue(47), 
    calcPowerValue(48), calcPowerValue(49), calcPowerValue(50), calcPowerValue(51), calcPowerValue(52), calcPowerValue(53), calcPowerValue(54), calcPowerValue(55), 
    calcPowerValue(56), calcPowerValue(57), calcPowerValue(58), calcPowerValue(59), calcPowerValue(60), calcPowerValue(61), calcPowerValue(62), calcPowerValue(63), 
    calcPowerValue(64), calcPowerValue(65), calcPowerValue(66), calcPowerValue(67), calcPowerValue(68), calcPowerValue(69), calcPowerValue(70), calcPowerValue(71), 
    calcPowerValue(72), calcPowerValue(73), calcPowerValue(74), calcPowerValue(75), calcPowerValue(76), calcPowerValue(77), calcPowerValue(78), calcPowerValue(79), 
    calcPowerValue(80), calcPowerValue(81), calcPowerValue(82), calcPowerValue(83), calcPowerValue(84), calcPowerValue(85), calcPowerValue(86), calcPowerValue(87), 
    calcPowerValue(88), calcPowerValue(89), calcPowerValue(90), calcPowerValue(91), calcPowerValue(92), calcPowerValue(93), calcPowerValue(94), calcPowerValue(95), 
    calcPowerValue(96), calcPowerValue(97), calcPowerValue(98), calcPowerValue(99), calcPowerValue(100), calcPowerValue(101), calcPowerValue(102), calcPowerValue(103), 
    calcPowerValue(104), calcPowerValue(105), calcPowerValue(106), calcPowerValue(107), calcPowerValue(108), calcPowerValue(109), calcPowerValue(110), calcPowerValue(111), 
    calcPowerValue(112), calcPowerValue(113), calcPowerValue(114), calcPowerValue(115), calcPowerValue(116), calcPowerValue(117), calcPowerValue(118), calcPowerValue(119), 
    calcPowerValue(120), calcPowerValue(121), calcPowerValue(122), calcPowerValue(123), calcPowerValue(124), calcPowerValue(125), calcPowerValue(126), calcPowerValue(127), 
    calcPowerValue(128), calcPowerValue(129), calcPowerValue(130), calcPowerValue(131), calcPowerValue(132), calcPowerValue(133), calcPowerValue(134), calcPowerValue(135), 
    calcPowerValue(136), calcPowerValue(137), calcPowerValue(138), calcPowerValue(139), calcPowerValue(140), calcPowerValue(141), calcPowerValue(142), calcPowerValue(143), 
    calcPowerValue(144), calcPowerValue(145), calcPowerValue(146), calcPowerValue(147), calcPowerValue(148), calcPowerValue(149), calcPowerValue(150), calcPowerValue(151), 
    calcPowerValue(152), calcPowerValue(153), calcPowerValue(154), calcPowerValue(155), calcPowerValue(156), calcPowerValue(157), calcPowerValue(158), calcPowerValue(159), 
    calcPowerValue(160), calcPowerValue(161), calcPowerValue(162), calcPowerValue(163), calcPowerValue(164), calcPowerValue(165), calcPowerValue(166), calcPowerValue(167), 
    calcPowerValue(168), calcPowerValue(169), calcPowerValue(170), calcPowerValue(171), calcPowerValue(172), calcPowerValue(173), calcPowerValue(174), calcPowerValue(175), 
    calcPowerValue(176), calcPowerValue(177), calcPowerValue(178), calcPowerValue(179), calcPowerValue(180), calcPowerValue(181), calcPowerValue(182), calcPowerValue(183), 
    calcPowerValue(184), calcPowerValue(185), calcPowerValue(186), calcPowerValue(187), calcPowerValue(188), calcPowerValue(189), calcPowerValue(190), calcPowerValue(191), 
    calcPowerValue(192), calcPowerValue(193), calcPowerValue(194), calcPowerValue(195), calcPowerValue(196), calcPowerValue(197), calcPowerValue(198), calcPowerValue(199), 
    calcPowerValue(200), calcPowerValue(201), calcPowerValue(202), calcPowerValue(203), calcPowerValue(204), calcPowerValue(205), calcPowerValue(206), calcPowerValue(207), 
    calcPowerValue(208), calcPowerValue(209), calcPowerValue(210), calcPowerValue(211), calcPowerValue(212), calcPowerValue(213), calcPowerValue(214), calcPowerValue(215), 
    calcPowerValue(216), calcPowerValue(217), calcPowerValue(218), calcPowerValue(219), calcPowerValue(220), calcPowerValue(221), calcPowerValue(222), calcPowerValue(223), 
    calcPowerValue(224), calcPowerValue(225), calcPowerValue(226), calcPowerValue(227), calcPowerValue(228), calcPowerValue(229), calcPowerValue(230), calcPowerValue(231), 
    calcPowerValue(232), calcPowerValue(233), calcPowerValue(234), calcPowerValue(235), calcPowerValue(236), calcPowerValue(237), calcPowerValue(238), calcPowerValue(239), 
    calcPowerValue(240), calcPowerValue(241), calcPowerValue(242), calcPowerValue(243), calcPowerValue(244), calcPowerValue(245), calcPowerValue(246), calcPowerValue(247), 
    calcPowerValue(248), calcPowerValue(249), calcPowerValue(250), calcPowerValue(251), calcPowerValue(252), calcPowerValue(253), calcPowerValue(254), calcPowerValue(255), 
};
#endif
constexpr uint8_t getPowerValue(uint8_t brightness) {
    return POWERTABLE[brightness];
}
#endif // !FASTLED_POWER_SCALING_COMPUTE
#endif // !FASTLED_POWER_SCALING_FAST



uint32_t calculate_unscaled_power_mW( const CRGB* ledbuffer, uint16_t numLeds ) //25354
{
    uint32_t red32 = 0, green32 = 0, blue32 = 0;
    const CRGB* firstled = &(ledbuffer[0]);
    uint8_t* p = (uint8_t*)(firstled);

    uint16_t count = numLeds;

    // This loop might benefit from an AVR assembly version -MEK
    while(count) {
        red32   += getPowerValue(*p++);
        green32 += getPowerValue(*p++);
        blue32  += getPowerValue(*p++);
        count--;
    }

    red32   *= gRed_mW;
    green32 *= gGreen_mW;
    blue32  *= gBlue_mW;

    red32   >>= 8;
    green32 >>= 8;
    blue32  >>= 8;

    uint32_t total = red32 + green32 + blue32 + (gDark_mW * numLeds);

    return total;
}


uint8_t calculate_max_brightness_for_power_vmA(const CRGB* ledbuffer, uint16_t numLeds, uint8_t target_brightness, uint32_t max_power_V, uint32_t max_power_mA) {
	return calculate_max_brightness_for_power_mW(ledbuffer, numLeds, target_brightness, max_power_V * max_power_mA);
}

uint8_t calculate_max_brightness_for_power_mW(const CRGB* ledbuffer, uint16_t numLeds, uint8_t target_brightness, uint32_t max_power_mW) {
 	uint32_t total_mW = calculate_unscaled_power_mW( ledbuffer, numLeds);

	uint32_t requested_power_mW = ((uint32_t)total_mW * target_brightness) / 256;

	uint8_t recommended_brightness = target_brightness;
	if(requested_power_mW > max_power_mW) { 
    		recommended_brightness = (uint32_t)((uint8_t)(target_brightness) * (uint32_t)(max_power_mW)) / ((uint32_t)(requested_power_mW));
	}

	return recommended_brightness;
}

// sets brightness to
//  - no more than target_brightness
//  - no more than max_mW milliwatts
uint8_t calculate_max_brightness_for_power_mW( uint8_t target_brightness, uint32_t max_power_mW)
{
    uint8_t target_brightness_scaled = getPowerValue(target_brightness);

    uint32_t total_mW = gMCU_mW;

    CLEDController *pCur = CLEDController::head();
	while(pCur) {
        total_mW += calculate_unscaled_power_mW( pCur->leds(), pCur->size());
		pCur = pCur->next();
	}

#if POWER_DEBUG_PRINT == 1
    Serial.print("power demand at full brightness mW = ");
    Serial.println( total_mW);
#endif

    uint32_t requested_power_mW = ((uint32_t)total_mW * target_brightness_scaled) / 256;
#if POWER_DEBUG_PRINT == 1
    if( target_brightness != 255 ) {
        Serial.print("power demand at scaled brightness mW = ");
        Serial.println( requested_power_mW);
    }
    Serial.print("power limit mW = ");
    Serial.println( max_power_mW);
#endif

    if( requested_power_mW < max_power_mW) {
#if POWER_LED > 0
        if( gMaxPowerIndicatorLEDPinNumber ) {
            Pin(gMaxPowerIndicatorLEDPinNumber).lo(); // turn the LED off
        }
#endif
#if POWER_DEBUG_PRINT == 1
        Serial.print("demand is under the limit");
#endif
        return target_brightness;
    }

    uint8_t recommended_brightness = (uint32_t)(target_brightness_scaled * max_power_mW) / requested_power_mW;
#if POWER_DEBUG_PRINT == 1
    Serial.print("recommended brightness # = ");
    Serial.println( recommended_brightness);

    uint32_t resultant_power_mW = (total_mW * recommended_brightness) / 256;
    Serial.print("resultant power demand mW = ");
    Serial.println( resultant_power_mW);

    Serial.println();
#endif

#if POWER_LED > 0
    if( gMaxPowerIndicatorLEDPinNumber ) {
        Pin(gMaxPowerIndicatorLEDPinNumber).hi(); // turn the LED on
    }
#endif

    return getReversePowerValue(recommended_brightness);
}


void set_max_power_indicator_LED( uint8_t pinNumber)
{
    gMaxPowerIndicatorLEDPinNumber = pinNumber;
}

void set_max_power_in_volts_and_milliamps( uint8_t volts, uint32_t milliamps)
{
  FastLED.setMaxPowerInVoltsAndMilliamps(volts, milliamps);
}

void set_max_power_in_milliwatts( uint32_t powerInmW)
{
  FastLED.setMaxPowerInMilliWatts(powerInmW);
}

void show_at_max_brightness_for_power()
{
  // power management usage is now in FastLED.show, no need for this function
  FastLED.show();
}

void delay_at_max_brightness_for_power( uint16_t ms)
{
  FastLED.delay(ms);
}

FASTLED_NAMESPACE_END
