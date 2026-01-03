#pragma once

// ok no namespace fl - Platform dispatch header only
// This header dispatches to platform-specific implementations

#if defined(ESP32)
    #include "platforms/esp/32/init_channel_engine.h"
#else
    #include "platforms/shared/init_channel_engine.h"
#endif
