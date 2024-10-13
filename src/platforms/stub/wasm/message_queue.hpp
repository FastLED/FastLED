
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

    bool messages_available() const;
    bool message_pop_front(std::string* message);
    bool message_push_back(const char* msg);
    size_t get_missed_messages_count() const;
    size_t get_message_count() const;
    size_t get_max_message_count() const;

private:
    std::deque<std::string> message_queue;
    size_t missed_messages_count;
};

MessageQueueImpl::MessageQueueImpl()
    : missed_messages_count(0) {}

bool MessageQueueImpl::messages_available() const {
    return !message_queue.empty();
}

bool MessageQueueImpl::message_pop_front(std::string* message) {
    if (message_queue.empty()) {
        return false;
    }
    *message = message_queue.front();
    message_queue.pop_front();
    return true;
}

bool MessageQueueImpl::message_push_back(const char* msg) {
    if (message_queue.size() >= FASTLED_WASM_MAX_MESSAGE_QUEUE_SIZE) {
        message_queue.pop_front();
        missed_messages_count++;
    }
    message_queue.push_back(msg);
    return true;
}

size_t MessageQueueImpl::get_missed_messages_count() const {
    return missed_messages_count;
}

size_t MessageQueueImpl::get_message_count() const {
    return message_queue.size();
}

size_t MessageQueueImpl::get_max_message_count() const {
    return FASTLED_WASM_MAX_MESSAGE_QUEUE_SIZE;
}


MessageQueue& MessageQueue::Instance() {
    return Singleton<MessageQueueImpl>::instance();
}

FASTLED_NAMESPACE_END

