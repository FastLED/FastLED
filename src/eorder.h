#pragma once
#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

/// RGB color channel orderings, used when instantiating controllers to determine
/// what order the controller should send data out in. The default ordering
/// is RGB.
/// Within this enum, the red channel is 0, the green channel is 1, and the
/// blue chanel is 2.
enum EOrder {
	RGB=0012,  ///< Red,   Green, Blue  (0012)
	RBG=0021,  ///< Red,   Blue,  Green (0021)
	GRB=0102,  ///< Green, Red,   Blue  (0102)
	GBR=0120,  ///< Green, Blue,  Red   (0120)
	BRG=0201,  ///< Blue,  Red,   Green (0201)
	BGR=0210   ///< Blue,  Green, Red   (0210)
};

FASTLED_NAMESPACE_END

