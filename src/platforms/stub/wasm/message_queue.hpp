
#pragma once

#include <deque>

#include "namespace.h"
#include "message_queue.h"
#include "singleton.h"


FASTLED_NAMESPACE_BEGIN

#ifndef FASTLED_WASM_MAX_MESSAGE_QUEUE_SIZE
#define FASTLED_WASM_MAX_MESSAGE_QUEUE_SIZE 100
#endif


class MessageQueueImpl : public MessageQueue {
public:
    MessageQueueImpl();

    bool available() const;
    bool popFront(std::string* message);
    bool pushBack(const char* msg);
    size_t getMissedCount() const;
    size_t getCount() const;
    size_t getMaxCount() const;

private:
    std::deque<std::string> queue;
    size_t missed_count;
};

MessageQueueImpl::MessageQueueImpl()
    : missed_count(0) {}

bool MessageQueueImpl::available() const {
    return !queue.empty();
}

bool MessageQueueImpl::popFront(std::string* message) {
    if (queue.empty()) {
        return false;
    }
    *message = queue.front();
    queue.pop_front();
    return true;
}

bool MessageQueueImpl::pushBack(const char* msg) {
    if (queue.size() >= FASTLED_WASM_MAX_MESSAGE_QUEUE_SIZE) {
        queue.pop_front();
        missed_count++;
    }
    queue.push_back(msg);
    return true;
}

size_t MessageQueueImpl::getMissedCount() const {
    return missed_count;
}

size_t MessageQueueImpl::getCount() const {
    return queue.size();
}

size_t MessageQueueImpl::getMaxCount() const {
    return FASTLED_WASM_MAX_MESSAGE_QUEUE_SIZE;
}


MessageQueue& MessageQueue::Instance() {
    return Singleton<MessageQueueImpl>::instance();
}

FASTLED_NAMESPACE_END

