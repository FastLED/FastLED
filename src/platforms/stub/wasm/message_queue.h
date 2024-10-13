#pragma once

#ifndef __EMSCRIPTEN__
#error "This file should only be included in an Emscripten build"
#endif

#include <string>
#include <deque>
#include "namespace.h"
#include <emscripten/emscripten.h>

FASTLED_NAMESPACE_BEGIN

class MessageQueue {
public:
    MessageQueue(size_t max_size = 100);

    bool messages_available() const;
    bool message_pop_front(std::string* message);
    bool message_push_back(const char* msg);
    size_t get_missed_messages_count() const;
    size_t get_message_count() const;
    size_t get_max_message_count() const;

private:
    std::deque<std::string> message_queue;
    const size_t MAX_QUEUE_SIZE;
    size_t missed_messages_count;
};

FASTLED_NAMESPACE_END
