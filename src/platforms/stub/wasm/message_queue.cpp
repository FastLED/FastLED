
#ifdef __EMSCRIPTEN__

#include "namespace.h"
#include "message_queue.h"

FASTLED_NAMESPACE_BEGIN

MessageQueue::MessageQueue(size_t max_size)
    : MAX_QUEUE_SIZE(max_size), missed_messages_count(0) {}

bool MessageQueue::messages_available() const {
    return !message_queue.empty();
}

bool MessageQueue::message_pop_front(std::string* message) {
    if (message_queue.empty()) {
        return false;
    }
    *message = message_queue.front();
    message_queue.pop_front();
    return true;
}

bool MessageQueue::message_push_back(const char* msg) {
    if (message_queue.size() >= MAX_QUEUE_SIZE) {
        message_queue.pop_front();
        missed_messages_count++;
    }
    message_queue.push_back(msg);
    return true;
}

size_t MessageQueue::get_missed_messages_count() const {
    return missed_messages_count;
}

size_t MessageQueue::get_message_count() const {
    return message_queue.size();
}

size_t MessageQueue::get_max_message_count() const {
    return MAX_QUEUE_SIZE;
}

FASTLED_NAMESPACE_END

#endif