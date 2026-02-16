#include "fl/remote/transport/http/connection.h"
#include "fl/remote/transport/http/connection.cpp.hpp"

#include "test.h"

using namespace fl;

FL_TEST_CASE("HttpConnection - Initial state is DISCONNECTED") {
    HttpConnection conn;
    FL_CHECK(conn.getState() == ConnectionState::DISCONNECTED);
    FL_CHECK(conn.isDisconnected());
    FL_CHECK_FALSE(conn.isConnected());
    FL_CHECK_FALSE(conn.shouldReconnect());
}

FL_TEST_CASE("HttpConnection - Connect transitions to CONNECTING") {
    HttpConnection conn;
    conn.connect();
    FL_CHECK(conn.getState() == ConnectionState::CONNECTING);
}

FL_TEST_CASE("HttpConnection - onConnected transitions to CONNECTED") {
    HttpConnection conn;
    conn.connect();
    conn.onConnected();
    FL_CHECK(conn.getState() == ConnectionState::CONNECTED);
    FL_CHECK(conn.isConnected());
}

FL_TEST_CASE("HttpConnection - Disconnect from CONNECTED") {
    HttpConnection conn;
    conn.connect();
    conn.onConnected();
    conn.disconnect();
    FL_CHECK(conn.getState() == ConnectionState::DISCONNECTED);
}

FL_TEST_CASE("HttpConnection - onDisconnected triggers RECONNECTING") {
    HttpConnection conn;
    conn.connect();
    conn.onConnected();
    conn.onDisconnected();
    FL_CHECK(conn.getState() == ConnectionState::RECONNECTING);
    FL_CHECK(conn.shouldReconnect());
}

FL_TEST_CASE("HttpConnection - Exponential backoff calculation") {
    ConnectionConfig config;
    config.reconnectInitialDelayMs = 1000;
    config.reconnectMaxDelayMs = 30000;
    config.reconnectBackoffMultiplier = 2;

    HttpConnection conn(config);
    conn.connect();

    // Rapid disconnect without successful connection establishes
    // First disconnect: delay = 1000ms (attempts = 0 → 1)
    conn.onDisconnected();
    FL_CHECK(conn.getState() == ConnectionState::RECONNECTING);
    FL_CHECK(conn.getReconnectAttempts() == 1);
    FL_CHECK(conn.getReconnectDelayMs() == 1000);

    // Second disconnect: delay = 2000ms (attempts = 1 → 2)
    conn.connect();
    conn.onDisconnected();
    FL_CHECK(conn.getReconnectAttempts() == 2);
    FL_CHECK(conn.getReconnectDelayMs() == 2000);

    // Third disconnect: delay = 4000ms (attempts = 2 → 3)
    conn.connect();
    conn.onDisconnected();
    FL_CHECK(conn.getReconnectAttempts() == 3);
    FL_CHECK(conn.getReconnectDelayMs() == 4000);

    // Fourth disconnect: delay = 8000ms (attempts = 3 → 4)
    conn.connect();
    conn.onDisconnected();
    FL_CHECK(conn.getReconnectAttempts() == 4);
    FL_CHECK(conn.getReconnectDelayMs() == 8000);
}

FL_TEST_CASE("HttpConnection - Max backoff delay cap") {
    ConnectionConfig config;
    config.reconnectInitialDelayMs = 1000;
    config.reconnectMaxDelayMs = 5000;  // Cap at 5s
    config.reconnectBackoffMultiplier = 2;

    HttpConnection conn(config);

    // Simulate multiple disconnects
    for (int i = 0; i < 10; i++) {
        conn.connect();
        conn.onConnected();
        conn.onDisconnected();
    }

    // Delay should be capped at 5000ms
    FL_CHECK(conn.getReconnectDelayMs() <= 5000);
}

FL_TEST_CASE("HttpConnection - Max reconnect attempts limit") {
    ConnectionConfig config;
    config.maxReconnectAttempts = 3;  // Only 3 attempts

    HttpConnection conn(config);

    // Initial connection and disconnect (attempt 1)
    conn.connect();
    conn.onConnected();
    conn.onDisconnected();
    FL_CHECK(conn.getState() == ConnectionState::RECONNECTING);
    FL_CHECK(conn.getReconnectAttempts() == 1);

    // Second failed reconnection attempt (attempt 2)
    conn.connect();  // Move to CONNECTING
    conn.onDisconnected();  // Disconnect again
    FL_CHECK(conn.getState() == ConnectionState::RECONNECTING);
    FL_CHECK(conn.getReconnectAttempts() == 2);

    // Third failed reconnection attempt (attempt 3)
    conn.connect();  // Move to CONNECTING
    conn.onDisconnected();  // Disconnect again
    FL_CHECK(conn.getState() == ConnectionState::RECONNECTING);
    FL_CHECK(conn.getReconnectAttempts() == 3);

    // Fourth failed reconnection attempt -> DISCONNECTED (max attempts reached)
    conn.connect();  // Move to CONNECTING
    conn.onDisconnected();  // Disconnect again -> should be DISCONNECTED
    FL_CHECK(conn.getState() == ConnectionState::DISCONNECTED);
    // Attempts counter should be reset to 0 when transitioning to DISCONNECTED
    FL_CHECK(conn.getReconnectAttempts() == 0);
}

FL_TEST_CASE("HttpConnection - Heartbeat should send after interval") {
    ConnectionConfig config;
    config.heartbeatIntervalMs = 1000;  // 1 second

    HttpConnection conn(config);
    conn.connect();
    conn.onConnected();

    // Initially should send heartbeat (never sent)
    FL_CHECK(conn.shouldSendHeartbeat(0));

    // After update, timestamp updated
    conn.update(0);

    // Before interval, should not send
    FL_CHECK_FALSE(conn.shouldSendHeartbeat(500));

    // After interval, should send
    FL_CHECK(conn.shouldSendHeartbeat(1000));
}

FL_TEST_CASE("HttpConnection - Connection timeout detection") {
    ConnectionConfig config;
    config.connectionTimeoutMs = 5000;  // 5 second timeout

    HttpConnection conn(config);
    conn.connect();
    conn.onConnected();
    conn.update(0);  // Initialize timestamps

    // Not timed out within timeout period
    FL_CHECK_FALSE(conn.isTimedOut(4000));

    // Timed out after timeout period
    FL_CHECK(conn.isTimedOut(5000));
}

FL_TEST_CASE("HttpConnection - Auto-reconnect on timeout") {
    ConnectionConfig config;
    config.connectionTimeoutMs = 5000;  // 5 second timeout

    HttpConnection conn(config);
    conn.connect();
    conn.onConnected();
    conn.update(0);  // Initialize timestamps

    // Update after timeout
    conn.update(6000);

    // Should transition to RECONNECTING
    FL_CHECK(conn.getState() == ConnectionState::RECONNECTING);
}

FL_TEST_CASE("HttpConnection - Reconnect attempt after delay") {
    ConnectionConfig config;
    config.reconnectInitialDelayMs = 1000;  // 1 second delay

    HttpConnection conn(config);
    conn.connect();
    conn.onConnected();
    conn.onDisconnected();

    FL_CHECK(conn.getState() == ConnectionState::RECONNECTING);

    // Before delay, still RECONNECTING
    conn.update(500);
    FL_CHECK(conn.getState() == ConnectionState::RECONNECTING);

    // After delay, should transition to CONNECTING
    conn.update(1000);
    FL_CHECK(conn.getState() == ConnectionState::CONNECTING);
}

FL_TEST_CASE("HttpConnection - Close permanently") {
    HttpConnection conn;
    conn.connect();
    conn.onConnected();
    conn.close();

    FL_CHECK(conn.getState() == ConnectionState::CLOSED);

    // Attempt to connect should be ignored
    conn.connect();
    FL_CHECK(conn.getState() == ConnectionState::CLOSED);
}

FL_TEST_CASE("HttpConnection - Successful reconnection resets attempts") {
    HttpConnection conn;
    conn.connect();
    conn.onConnected();

    // Disconnect 3 times
    for (int i = 0; i < 3; i++) {
        conn.onDisconnected();
        conn.connect();
        conn.onConnected();
    }

    // Reconnect attempts should be reset after successful connection
    FL_CHECK(conn.getReconnectAttempts() == 0);
}

FL_TEST_CASE("HttpConnection - onError behaves like onDisconnected") {
    HttpConnection conn;
    conn.connect();
    conn.onConnected();
    conn.onError();

    FL_CHECK(conn.getState() == ConnectionState::RECONNECTING);
}

FL_TEST_CASE("HttpConnection - Heartbeat not sent when disconnected") {
    ConnectionConfig config;
    config.heartbeatIntervalMs = 1000;

    HttpConnection conn(config);

    // Not connected, should not send heartbeat
    FL_CHECK_FALSE(conn.shouldSendHeartbeat(1000));

    // Still not connected after update
    conn.update(1000);
    FL_CHECK_FALSE(conn.shouldSendHeartbeat(2000));
}

FL_TEST_CASE("HttpConnection - No timeout when not connected") {
    ConnectionConfig config;
    config.connectionTimeoutMs = 1000;

    HttpConnection conn(config);

    // Disconnected, should never timeout
    FL_CHECK_FALSE(conn.isTimedOut(10000));
}
