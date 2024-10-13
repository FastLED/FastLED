#pragma once

#ifndef __EMSCRIPTEN__
#error "This file should only be included in an Emscripten build"
#endif

#include <string>

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

class MessageQueue {
    public:
        static MessageQueue& Instance();
        MessageQueue(const MessageQueue&) = delete;
        MessageQueue& operator=(const MessageQueue&) = delete;
        virtual ~MessageQueue() {}
        virtual bool messages_available() const = 0;
        virtual bool message_pop_front(std::string* message) = 0;
        virtual bool message_push_back(const char* msg) = 0;
        virtual size_t get_missed_messages_count() const = 0;
        virtual size_t get_message_count() const = 0;
        virtual size_t get_max_message_count() const = 0;
    protected:
        MessageQueue() {}
};


FASTLED_NAMESPACE_END
