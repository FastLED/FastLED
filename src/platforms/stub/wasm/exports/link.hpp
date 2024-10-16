#pragma once

#ifndef __EMSCRIPTEN__
#error "This file should only be included in an Emscripten build"
#endif

#include "timer.hpp"
#include "engine_events.hpp"
#include "setup_and_loop.hpp"
#include "message_queue.hpp"
#include "post_message.hpp"