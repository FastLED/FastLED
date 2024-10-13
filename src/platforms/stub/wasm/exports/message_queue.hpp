
#pragma once

#include <deque>
#include <mutex>
#include <utility>  // for std::move

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
    mutable std::mutex mutex;
};

MessageQueueImpl::MessageQueueImpl()
    : missed_count(0) {}

bool MessageQueueImpl::available() const {
    std::lock_guard<std::mutex> lock(mutex);
    return !queue.empty();
}

bool MessageQueueImpl::popFront(std::string* message) {
    std::lock_guard<std::mutex> lock(mutex);
    if (queue.empty()) {
        return false;
    }
    *message = std::move(queue.front());
    queue.pop_front();
    return true;
}

bool MessageQueueImpl::pushBack(const char* msg) {
    std::lock_guard<std::mutex> lock(mutex);
    if (queue.size() >= FASTLED_WASM_MAX_MESSAGE_QUEUE_SIZE) {
        queue.pop_front();
        missed_count++;
    }
    queue.push_back(msg);
    return true;
}

size_t MessageQueueImpl::getMissedCount() const {
    std::lock_guard<std::mutex> lock(mutex);
    return missed_count;
}

size_t MessageQueueImpl::getCount() const {
    std::lock_guard<std::mutex> lock(mutex);
    return queue.size();
}

size_t MessageQueueImpl::getMaxCount() const {
    return FASTLED_WASM_MAX_MESSAGE_QUEUE_SIZE;
}


MessageQueue& MessageQueue::Instance() {
    return Singleton<MessageQueueImpl>::instance();
}

FASTLED_NAMESPACE_END

