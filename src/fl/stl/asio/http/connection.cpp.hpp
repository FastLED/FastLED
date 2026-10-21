#pragma once

#include "fl/stl/asio/http/connection.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/noexcept.h"
namespace fl {

HttpConnection::HttpConnection(const ConnectionConfig& config)
    FL_NO_EXCEPT : mConfig(config)
    , mState(ConnectionState::DISCONNECTED)
    , mReconnectAttempts(0)
    , mReconnectDelayMs(0)
    , mNextReconnectTimeMs(0)
    , mWasReconnecting(false)
    , mLastHeartbeatSentMs(0)
    , mLastDataReceivedMs(0)
{
}

ConnectionState HttpConnection::getState() const FL_NO_EXCEPT {
    return mState;
}

bool HttpConnection::isConnected() const FL_NO_EXCEPT {
    return mState == ConnectionState::CONNECTED;
}

bool HttpConnection::isDisconnected() const FL_NO_EXCEPT {
    return mState == ConnectionState::DISCONNECTED;
}

bool HttpConnection::shouldReconnect() const FL_NO_EXCEPT {
    return mState == ConnectionState::RECONNECTING;
}

void HttpConnection::connect() FL_NO_EXCEPT {
    if (mState == ConnectionState::CLOSED) {
        return;  // Permanently closed, no connect allowed
    }

    // Track if we're coming from RECONNECTING state
    mWasReconnecting = (mState == ConnectionState::RECONNECTING);
    transitionTo(ConnectionState::CONNECTING, 0);
}

void HttpConnection::disconnect() FL_NO_EXCEPT {
    if (mState == ConnectionState::CONNECTED || mState == ConnectionState::CONNECTING ||
        mState == ConnectionState::RECONNECTING) {
        transitionTo(ConnectionState::DISCONNECTED, 0);
    }
}

void HttpConnection::close() FL_NO_EXCEPT {
    transitionTo(ConnectionState::CLOSED, 0);
}

void HttpConnection::onConnected(u32 currentTimeMs) FL_NO_EXCEPT {
    if (mState == ConnectionState::CONNECTING || mState == ConnectionState::RECONNECTING) {
        // Reset attempts if we were in RECONNECTING state before entering CONNECTING
        // This detects successful reconnection after disconnect
        bool shouldResetAttempts = mWasReconnecting || (mState == ConnectionState::RECONNECTING);

        transitionTo(ConnectionState::CONNECTED, currentTimeMs);

        if (shouldResetAttempts) {
            resetReconnectAttempts();  // Successful reconnection, reset counter
        } else {
            resetReconnectState();  // Initial connection, just reset timing
        }

        mWasReconnecting = false;
    }
}

void HttpConnection::onDisconnected() FL_NO_EXCEPT {
    if (mState == ConnectionState::CONNECTED || mState == ConnectionState::CONNECTING) {
        // Check if we should attempt reconnection
        // Use <= to handle the exact limit correctly (0-indexed attempts vs 1-indexed limit)
        if (mConfig.maxReconnectAttempts == 0 ||
            mReconnectAttempts < mConfig.maxReconnectAttempts) {
            transitionTo(ConnectionState::RECONNECTING, 0);
        } else {
            transitionTo(ConnectionState::DISCONNECTED, 0);
        }
    }
}

void HttpConnection::onError() FL_NO_EXCEPT {
    // Treat errors the same as disconnection
    onDisconnected();
}

void HttpConnection::onEvent(const asio::error_code& ec, u32 currentTimeMs) FL_NO_EXCEPT {
    if (ec.ok()) {
        onConnected(currentTimeMs);
    } else if (ec.code == asio::errc::eof || ec.code == asio::errc::connection_reset) {
        onDisconnected();
    } else {
        onError();
    }
}

void HttpConnection::onHeartbeatSent() FL_NO_EXCEPT {
    // Note: currentTimeMs should be passed, but for simplicity we don't update timestamp here
    // The caller should update mLastHeartbeatSentMs via update()
}

void HttpConnection::onHeartbeatReceived() FL_NO_EXCEPT {
    // Note: currentTimeMs should be passed, but for simplicity we don't update timestamp here
    // The caller should update mLastDataReceivedMs via update()
}

bool HttpConnection::shouldSendHeartbeat(u32 currentTimeMs) const FL_NO_EXCEPT {
    if (mState != ConnectionState::CONNECTED) {
        return false;  // Only send heartbeats when connected
    }

    // Check if enough time has passed since last heartbeat
    u32 timeSinceLastHeartbeat = currentTimeMs - mLastHeartbeatSentMs;
    return timeSinceLastHeartbeat >= mConfig.heartbeatIntervalMs;
}

void HttpConnection::update(u32 currentTimeMs) FL_NO_EXCEPT {
    // Check for timeout first (before updating timestamps)
    if (isTimedOut(currentTimeMs)) {
        onDisconnected();
        return;
    }

    // Handle reconnection
    if (mState == ConnectionState::RECONNECTING) {
        if (currentTimeMs >= mNextReconnectTimeMs) {
            // Time to attempt reconnection
            connect();  // Transition to CONNECTING
        }
    }

    // Update heartbeat sent timestamp if needed
    if (shouldSendHeartbeat(currentTimeMs)) {
        mLastHeartbeatSentMs = currentTimeMs;
    }
}

u32 HttpConnection::getReconnectDelayMs() const FL_NO_EXCEPT {
    return mReconnectDelayMs;
}

u32 HttpConnection::getReconnectAttempts() const FL_NO_EXCEPT {
    return mReconnectAttempts;
}

bool HttpConnection::isTimedOut(u32 currentTimeMs) const FL_NO_EXCEPT {
    if (mState != ConnectionState::CONNECTED) {
        return false;  // Only check timeout when connected
    }

    // Check if enough time has passed since last data received
    u32 timeSinceLastData = currentTimeMs - mLastDataReceivedMs;
    return timeSinceLastData >= mConfig.connectionTimeoutMs;
}

void HttpConnection::transitionTo(ConnectionState newState, u32 currentTimeMs) FL_NO_EXCEPT {
    mState = newState;

    // Handle state-specific initialization
    switch (newState) {
        case ConnectionState::DISCONNECTED:
            resetReconnectAttempts();  // Full reset on user disconnect
            break;

        case ConnectionState::CONNECTING:
            // Starting connection attempt
            break;

        case ConnectionState::CONNECTED:
            // Initialize data received timestamp (for timeout detection)
            mLastDataReceivedMs = currentTimeMs;
            // Initialize heartbeat timestamp to trigger immediate send on first check
            // Subtract interval from currentTimeMs to ensure timeSince >= interval
            if (mLastHeartbeatSentMs == 0) {
                // Use wrapping arithmetic: currentTimeMs - interval
                // Even if this underflows (currentTimeMs < interval), the unsigned
                // subtraction in shouldSendHeartbeat will wrap correctly
                mLastHeartbeatSentMs = currentTimeMs - mConfig.heartbeatIntervalMs;
            }
            break;

        case ConnectionState::RECONNECTING:
            // Calculate backoff delay BEFORE incrementing attempts
            mReconnectDelayMs = calculateBackoffDelay();
            mNextReconnectTimeMs = currentTimeMs + mReconnectDelayMs;
            // Increment attempts for next reconnection
            mReconnectAttempts++;
            break;

        case ConnectionState::CLOSED:
            resetReconnectAttempts();  // Full reset on close
            break;
    }
}

void HttpConnection::resetReconnectState() FL_NO_EXCEPT {
    // Reset reconnection timing state, but keep attempts counter
    mReconnectDelayMs = 0;
    mNextReconnectTimeMs = 0;
}

void HttpConnection::resetReconnectAttempts() FL_NO_EXCEPT {
    // Fully reset all reconnection state including attempts counter
    mReconnectAttempts = 0;
    resetReconnectState();
}

u32 HttpConnection::calculateBackoffDelay() const FL_NO_EXCEPT {
    // Exponential backoff: delay = initial * (multiplier ^ attempts)
    u32 delay = mConfig.reconnectInitialDelayMs;
    for (u32 i = 0; i < mReconnectAttempts; i++) {
        delay *= mConfig.reconnectBackoffMultiplier;
        if (delay > mConfig.reconnectMaxDelayMs) {
            delay = mConfig.reconnectMaxDelayMs;
            break;
        }
    }

    return delay;
}

} // namespace fl
