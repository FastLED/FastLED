
#include "server_thread.h"

#include "fl/remote/transport/http/stream_server.h"
#include "fl/stl/atomic.h"
#include "fl/stl/chrono.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/stdint.h"
#include "fl/stl/thread.h"

namespace fl {

struct ServerThreadData {
    fl::shared_ptr<HttpStreamServer> mServer;
    std::atomic<bool> mRunning;  // okay std namespace
    std::thread mThread;  // okay std namespace
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
        std::this_thread::sleep_for(std::chrono::milliseconds(10));  // okay std namespace
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
    mData->mThread = std::thread(serverThreadFunc, mData); // okay std namespace

    // Give the thread time to start
    std::this_thread::sleep_for(                          // okay std namespace
        std::chrono::milliseconds(100));                  // okay std namespace

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
