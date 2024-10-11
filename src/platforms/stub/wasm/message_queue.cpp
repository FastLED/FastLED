#ifdef __EMSCRIPTEN__

#include <deque>  // ok include
#include "message_queue.h"

FASTLED_NAMESPACE_BEGIN

static std::deque<std::string> message_queue;

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

bool js_message_push_back(const std::string& message) {
    message_queue.push_back(message);
    return true;
}

FASTLED_NAMESPACE_END

#endif