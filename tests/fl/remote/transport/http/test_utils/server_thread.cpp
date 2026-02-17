
#include "server_thread.h"

#include "fl/remote/transport/http/stream_server.h"
#include "fl/stl/atomic.h"
#include "fl/stl/chrono.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/stdint.h"
#include "fl/stl/thread.h"
#include <atomic> // ok include
#include <chrono> // ok include
#include <thread> // ok include

namespace fl {

struct ServerThreadData {
    fl::shared_ptr<HttpStreamServer> mServer;
    std::atomic<bool> mRunning;
    std::thread mThread;
};

ServerThread::ServerThread(fl::shared_ptr<HttpStreamServer> server) {
    mData = fl::make_shared<ServerThreadData>();
    mData->mServer = server;
    mData->mRunning = false;
}

ServerThread::~ServerThread() {
    stop();
}

bool ServerThread::start() {
    if (mData->mRunning.load()) {
        return true;
    }

    if (!mData->mServer) {
        return false;
    }

    mData->mRunning.store(true);
    mData->mThread = std::thread(&ServerThread::run, this); // ok std namespace

    // Give the thread time to start
    std::this_thread::sleep_for(
        std::chrono::milliseconds(100)); // ok std namespace

    return true;
}

void ServerThread::stop() {
    if (!mData->mRunning.load()) {
        return;
    }

    mData->mRunning.store(false);

    if (mData->mThread.joinable()) {
        mData->mThread.join();
    }
}

bool ServerThread::isRunning() const { return mData->mRunning.load(); }

void ServerThread::run() {
    while (mData->mRunning.load()) {
        uint32_t now = fl::millis();

        // Accept new clients (non-blocking)
        mData->mServer->acceptClients();

        // Update server (handles disconnections, heartbeat, etc.)
        mData->mServer->update(now);

        // Small sleep to prevent CPU spinning
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

} // namespace fl
