
#include "server_thread.h"

#include "fl/net/http/stream_server.h"
#include "fl/stl/atomic.h"
#include "fl/stl/chrono.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/stdint.h"
#include "fl/stl/thread.h"

namespace fl {

struct ServerThreadData {
    fl::shared_ptr<HttpStreamServer> mServer;
    fl::atomic<bool> mRunning;
    fl::thread mThread;
};

// Static thread function that captures shared_ptr to data, not raw `this`
static void serverThreadFunc(fl::shared_ptr<ServerThreadData> data) {
    while (data->mRunning.load()) {
        // Accept new clients (non-blocking)
        // NOTE: Do NOT call update() here - the test's main thread handles
        // data I/O via update()/readRequest(). Calling update() from both
        // threads causes concurrent socket reads that corrupt chunked encoding.
        data->mServer->acceptClients();

        // Small sleep to prevent CPU spinning
        fl::this_thread::sleep_for(fl::chrono::milliseconds(10));  // ok sleep for
    }
}

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
    // Pass shared_ptr to data so the thread keeps it alive even if
    // the ServerThread object is destroyed before join completes.
    mData->mThread = fl::thread(serverThreadFunc, mData);

    // Give the thread time to start
    fl::this_thread::sleep_for(fl::chrono::milliseconds(100));  // ok sleep for

    return true;
}

void ServerThread::stop() {
    // Always set running to false and join, even if already false.
    mData->mRunning.store(false);

    if (mData->mThread.joinable()) {
        mData->mThread.join();
    }
}

bool ServerThread::isRunning() const { return mData->mRunning.load(); }

} // namespace fl
