#pragma once

#include <string>  // ok include
#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

/**
 * @brief Check if there are any messages available in the queue.
 * @return true if there are messages, false otherwise.
 */
bool js_messages_available();

/**
 * @brief Retrieve and remove the first message from the queue.
 * @param message Pointer to a string where the message will be stored.
 * @return true if a message was retrieved, false if the queue was empty.
 */
bool js_message_pop_front(std::string* message);

/**
 * @brief Add a new message to the back of the queue.
 * @param message The message to be added.
 * @return true if the message was successfully added, false otherwise.
 */
bool js_message_push_back(const std::string& message);

FASTLED_NAMESPACE_END
