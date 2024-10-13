#pragma once

#ifndef __EMSCRIPTEN__
#error "This file should only be included in an Emscripten build"
#endif

#include <string>

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

// Messages from Js to C++. You must pull the messages from the queue. 
class MessageQueue {
    public:
        static MessageQueue& Instance();
        MessageQueue(const MessageQueue&) = delete;
        MessageQueue& operator=(const MessageQueue&) = delete;
        virtual ~MessageQueue() {}
        virtual bool available() const = 0;
        virtual bool popFront(std::string* message) = 0;
        virtual bool pushBack(const char* msg) = 0;
        virtual size_t getMissedCount() const = 0;
        virtual size_t getCount() const = 0;
        virtual size_t getMaxCount() const = 0;
    protected:
        MessageQueue() {}
};


FASTLED_NAMESPACE_END
