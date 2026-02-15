#pragma once

namespace fl {

// =============================================================================
// RpcMode - Execution mode for RPC methods
// =============================================================================

enum class RpcMode {
    SYNC,   // Synchronous: Returns result immediately (default)
    ASYNC   // Asynchronous: Sends ACK immediately, result comes later
};

} // namespace fl
