#pragma once

#include <string>
#include <deque>
#include "namespace.h"
#include <emscripten/emscripten.h>

FASTLED_NAMESPACE_BEGIN

class MessageQueue {
public:
    MessageQueue(size_t max_size = 100) : MAX_QUEUE_SIZE(max_size), missed_messages_count(0) {}

    /**
     * @brief Check if there are any messages available in the queue.
     * @return true if there are messages, false otherwise.
     */
    bool messages_available() const {
        return !message_queue.empty();
    }

    /**
     * @brief Retrieve and remove the first message from the queue.
     * @param message Pointer to a string where the message will be stored.
     * @return true if a message was retrieved, false if the queue was empty.
     */
    bool message_pop_front(std::string* message) {
        if (message_queue.empty()) {
            return false;
        }
        *message = message_queue.front();
        message_queue.pop_front();
        return true;
    }

    /**
     * @brief Add a new message to the back of the queue.
     * @param message The message to be added.
     * @return true if the message was successfully added, false otherwise.
     */
    EMSCRIPTEN_KEEPALIVE bool message_push_back(const char* msg) {
        if (message_queue.size() >= MAX_QUEUE_SIZE) {
            message_queue.pop_front();
            missed_messages_count++;
        }
        message_queue.push_back(msg);
        return true;
    }

    /**
     * @brief Get the count of messages that were missed due to queue overflow.
     * @return The number of missed messages.
     */
    size_t get_missed_messages_count() const {
        return missed_messages_count;
    }

    /**
     * @brief Get the current number of messages in the queue.
     * @return The number of messages currently in the queue.
     */
    size_t get_message_count() const {
        return message_queue.size();
    }

    /**
     * @brief Get the maximum number of messages allowed in the queue.
     * @return The maximum number of messages allowed in the queue.
     */
    size_t get_max_message_count() const {
        return MAX_QUEUE_SIZE;
    }

private:
    std::deque<std::string> message_queue;
    const size_t MAX_QUEUE_SIZE;
    size_t missed_messages_count;
};

FASTLED_NAMESPACE_END
