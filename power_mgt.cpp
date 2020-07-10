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

constexpr float POWER_EXPONENENT = 0.77; // 50% pwm will result in 0.5^0.75 = 58.6% of power usage
static const uint8_t gRed_mW   = 26; // 16mA @ 5v = 80mW
static const uint8_t gGreen_mW = 16; // 11mA @ 5v = 55mW
static const uint8_t gBlue_mW  = 26; // 15mA @ 5v = 75mW
static const uint8_t gDark_mW  = 6; //  1mA @ 5v =  5mW

// Alternate calibration by RAtkins via pre-PSU wattage measurments;
// these are all probably about 20%-25% too high due to PSU heat losses,
// but if you're measuring wattage on the PSU input side, this may
// be a better set of calibrations.  (WS2812B)
//  static const uint8_t gRed_mW   = 100;
//  static const uint8_t gGreen_mW =  48;
//  static const uint8_t gBlue_mW  = 100;
//  static const uint8_t gDark_mW  =  12;


#define POWER_LED 1
#define POWER_DEBUG_PRINT 0


// Power consumed by the MCU
static const uint8_t gMCU_mW  =  25 * 5; // 25mA @ 5v = 125 mW

static uint8_t  gMaxPowerIndicatorLEDPinNumber = 0; // default = Arduino onboard LED pin.  set to zero to skip this.


constexpr uint8_t getPowerValue(const uint8_t& brightness) {
    return pow(brightness, POWER_EXPONENENT) * pow(256, 1 - POWER_EXPONENENT);
}
#if __cplusplus >= 201700L
// c++17
constexpr std::array<uint8_t, 256> POWERTABLE = [] {
  std::array<uint8_t, 256> A = {};
  for (unsigned i = 0; i < 256; i++) {
    A[i] = getPowerValue(i);
  }
  return A;
}();
#else
constexpr std::array<uint8_t, 256> POWERTABLE = {
    getPowerValue(0), getPowerValue(1), getPowerValue(2), getPowerValue(3), getPowerValue(4), getPowerValue(5), getPowerValue(6), getPowerValue(7), 
    getPowerValue(8), getPowerValue(9), getPowerValue(10), getPowerValue(11), getPowerValue(12), getPowerValue(13), getPowerValue(14), getPowerValue(15), 
    getPowerValue(16), getPowerValue(17), getPowerValue(18), getPowerValue(19), getPowerValue(20), getPowerValue(21), getPowerValue(22), getPowerValue(23), 
    getPowerValue(24), getPowerValue(25), getPowerValue(26), getPowerValue(27), getPowerValue(28), getPowerValue(29), getPowerValue(30), getPowerValue(31), 
    getPowerValue(32), getPowerValue(33), getPowerValue(34), getPowerValue(35), getPowerValue(36), getPowerValue(37), getPowerValue(38), getPowerValue(39), 
    getPowerValue(40), getPowerValue(41), getPowerValue(42), getPowerValue(43), getPowerValue(44), getPowerValue(45), getPowerValue(46), getPowerValue(47), 
    getPowerValue(48), getPowerValue(49), getPowerValue(50), getPowerValue(51), getPowerValue(52), getPowerValue(53), getPowerValue(54), getPowerValue(55), 
    getPowerValue(56), getPowerValue(57), getPowerValue(58), getPowerValue(59), getPowerValue(60), getPowerValue(61), getPowerValue(62), getPowerValue(63), 
    getPowerValue(64), getPowerValue(65), getPowerValue(66), getPowerValue(67), getPowerValue(68), getPowerValue(69), getPowerValue(70), getPowerValue(71), 
    getPowerValue(72), getPowerValue(73), getPowerValue(74), getPowerValue(75), getPowerValue(76), getPowerValue(77), getPowerValue(78), getPowerValue(79), 
    getPowerValue(80), getPowerValue(81), getPowerValue(82), getPowerValue(83), getPowerValue(84), getPowerValue(85), getPowerValue(86), getPowerValue(87), 
    getPowerValue(88), getPowerValue(89), getPowerValue(90), getPowerValue(91), getPowerValue(92), getPowerValue(93), getPowerValue(94), getPowerValue(95), 
    getPowerValue(96), getPowerValue(97), getPowerValue(98), getPowerValue(99), getPowerValue(100), getPowerValue(101), getPowerValue(102), getPowerValue(103), 
    getPowerValue(104), getPowerValue(105), getPowerValue(106), getPowerValue(107), getPowerValue(108), getPowerValue(109), getPowerValue(110), getPowerValue(111), 
    getPowerValue(112), getPowerValue(113), getPowerValue(114), getPowerValue(115), getPowerValue(116), getPowerValue(117), getPowerValue(118), getPowerValue(119), 
    getPowerValue(120), getPowerValue(121), getPowerValue(122), getPowerValue(123), getPowerValue(124), getPowerValue(125), getPowerValue(126), getPowerValue(127), 
    getPowerValue(128), getPowerValue(129), getPowerValue(130), getPowerValue(131), getPowerValue(132), getPowerValue(133), getPowerValue(134), getPowerValue(135), 
    getPowerValue(136), getPowerValue(137), getPowerValue(138), getPowerValue(139), getPowerValue(140), getPowerValue(141), getPowerValue(142), getPowerValue(143), 
    getPowerValue(144), getPowerValue(145), getPowerValue(146), getPowerValue(147), getPowerValue(148), getPowerValue(149), getPowerValue(150), getPowerValue(151), 
    getPowerValue(152), getPowerValue(153), getPowerValue(154), getPowerValue(155), getPowerValue(156), getPowerValue(157), getPowerValue(158), getPowerValue(159), 
    getPowerValue(160), getPowerValue(161), getPowerValue(162), getPowerValue(163), getPowerValue(164), getPowerValue(165), getPowerValue(166), getPowerValue(167), 
    getPowerValue(168), getPowerValue(169), getPowerValue(170), getPowerValue(171), getPowerValue(172), getPowerValue(173), getPowerValue(174), getPowerValue(175), 
    getPowerValue(176), getPowerValue(177), getPowerValue(178), getPowerValue(179), getPowerValue(180), getPowerValue(181), getPowerValue(182), getPowerValue(183), 
    getPowerValue(184), getPowerValue(185), getPowerValue(186), getPowerValue(187), getPowerValue(188), getPowerValue(189), getPowerValue(190), getPowerValue(191), 
    getPowerValue(192), getPowerValue(193), getPowerValue(194), getPowerValue(195), getPowerValue(196), getPowerValue(197), getPowerValue(198), getPowerValue(199), 
    getPowerValue(200), getPowerValue(201), getPowerValue(202), getPowerValue(203), getPowerValue(204), getPowerValue(205), getPowerValue(206), getPowerValue(207), 
    getPowerValue(208), getPowerValue(209), getPowerValue(210), getPowerValue(211), getPowerValue(212), getPowerValue(213), getPowerValue(214), getPowerValue(215), 
    getPowerValue(216), getPowerValue(217), getPowerValue(218), getPowerValue(219), getPowerValue(220), getPowerValue(221), getPowerValue(222), getPowerValue(223), 
    getPowerValue(224), getPowerValue(225), getPowerValue(226), getPowerValue(227), getPowerValue(228), getPowerValue(229), getPowerValue(230), getPowerValue(231), 
    getPowerValue(232), getPowerValue(233), getPowerValue(234), getPowerValue(235), getPowerValue(236), getPowerValue(237), getPowerValue(238), getPowerValue(239), 
    getPowerValue(240), getPowerValue(241), getPowerValue(242), getPowerValue(243), getPowerValue(244), getPowerValue(245), getPowerValue(246), getPowerValue(247), 
    getPowerValue(248), getPowerValue(249), getPowerValue(250), getPowerValue(251), getPowerValue(252), getPowerValue(253), getPowerValue(254), getPowerValue(255), 
};
#endif



uint32_t calculate_unscaled_power_mW( const CRGB* ledbuffer, uint16_t numLeds ) //25354
{
    uint32_t red32 = 0, green32 = 0, blue32 = 0;
    const CRGB* firstled = &(ledbuffer[0]);
    uint8_t* p = (uint8_t*)(firstled);

    uint16_t count = numLeds;

    // This loop might benefit from an AVR assembly version -MEK
    while(count) {
        red32   += POWERTABLE[*p++];
        green32 += POWERTABLE[*p++];
        blue32  += POWERTABLE[*p++];
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
    target_brightness = pow(target_brightness, POWER_EXPONENENT) * pow(256, 1 - POWER_EXPONENENT);

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

    uint32_t requested_power_mW = ((uint32_t)total_mW * target_brightness) / 256;
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
        return pow(target_brightness / pow(256, 1 - POWER_EXPONENENT), 1.f/POWER_EXPONENENT);
    }

    uint8_t recommended_brightness = (uint32_t)(target_brightness * max_power_mW) / requested_power_mW;
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

    return  pow(recommended_brightness / pow(256, 1 - POWER_EXPONENENT), 1.f/POWER_EXPONENENT);
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
