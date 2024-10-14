#pragma once



#ifndef __EMSCRIPTEN__
#error "This file should only be included in an Emscripten build"
#endif


#include "exports/timer.hpp"
#include "exports/engine_events.hpp"
#include "exports/setup_and_loop.hpp"
#include "exports/message_queue.hpp"
#include "exports/canvas_size.hpp"
#include "exports/post_message.hpp"

#include "ui/slider.hpp"