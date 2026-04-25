#pragma once

/// @file fl/ui.h
/// @brief Aggregator header for the fl/ui/ family of per-element UI types.
///
/// Each UI type lives in its own header under fl/ui/. This file re-exports
/// them so existing consumers that include "fl/ui.h" continue to work.

#include "fl/ui/audio.h"
#include "fl/ui/button.h"
#include "fl/ui/checkbox.h"
#include "fl/ui/description.h"
#include "fl/ui/dropdown.h"
#include "fl/ui/element.h"
#include "fl/ui/group.h"
#include "fl/ui/help.h"
#include "fl/ui/number_field.h"
#include "fl/ui/slider.h"
#include "fl/ui/title.h"
