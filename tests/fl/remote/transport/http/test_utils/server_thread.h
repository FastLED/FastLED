#pragma once

#include "fl/remote/transport/http/stream_server.h"
#include "fl/stl/shared_ptr.h"


namespace fl {

struct ServerThreadData;

/// Helper class to run HttpStreamServer in a background thread
/// This is necessary for same-process client+server scenarios where
/// the client's blocking connect() would otherwise deadlock
class ServerThread {
public:
    /// Constructor
    /// @param server Shared pointer to HttpStreamServer
    ServerThread(fl::shared_ptr<HttpStreamServer> server);

    /// Destructor - stops the server thread if running
    ~ServerThread();

    /// Start the server in a background thread
    /// @return true if started successfully, false otherwise
    bool start();

    /// Stop the server thread
    void stop();

    /// Check if the server thread is running
    /// @return true if running, false otherwise
    bool isRunning() const;

private:
    /// Server thread main loop
    void run();
    
    fl::shared_ptr<ServerThreadData> mData;

};

}  // namespace fl
