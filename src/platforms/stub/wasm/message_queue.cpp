#ifdef __EMSCRIPTEN__

#include <deque>  // ok include
#include "message_queue.h"

FASTLED_NAMESPACE_BEGIN

static std::deque<std::string> message_queue;
static const size_t MAX_QUEUE_SIZE = 100;
static size_t missed_messages_count = 0;

bool js_messages_available() {
    return !message_queue.empty();
}

bool js_message_pop_front(std::string* message) {
    if (message_queue.empty()) {
        return false;
    }
    *message = message_queue.front();
    message_queue.pop_front();
    return true;
}

extern "C" bool js_message_push_back(const char* msg) {
    if (message_queue.size() >= MAX_QUEUE_SIZE) {
        message_queue.pop_front();
        missed_messages_count++;
    }
    message_queue.push_back(message);
    return true;
}

size_t js_get_missed_messages_count() {
    return missed_messages_count;
}

size_t js_get_message_count() {
    return message_queue.size();
}

size_t js_get_max_message_count() {
    return MAX_QUEUE_SIZE;
}

FASTLED_NAMESPACE_END

#endif
