#ifndef TASK_RPCHANDLER_H
#define TASK_RPCHANDLER_H

#include <atomic>
#include <cstddef>
#include <memory>
#include <string_view>
#include <thread>

struct MessageHeader;

namespace task {
/**
 * Handle RPC requests for the task creation endpoint.
 *
 * This encapsulates the port on which we listen, as well as the thread that processes messages
 * received on it.
 */
class RpcHandler {
    public:
        /// Name of the task service
        static const std::string_view kPortName;

    public:
        static void init() {
            gShared = new RpcHandler;
        }

    private:
        RpcHandler();

        void main();

        /// Handlesa received message.
        bool handleMsg(const struct MessageHeader *msg, const size_t len);
        /// Handles a "create task" message
        bool handleTaskCreate(const struct MessageHeader *msg, const size_t len);

    private:
        static RpcHandler *gShared;

        /// handle of the task handler port
        uintptr_t portHandle;
        /// handle for the worker thread
        uintptr_t threadHandle;

        /// when set, the worker thread will continue executing
        std::atomic_bool run;
        /// the worker thread
        std::unique_ptr<std::thread> worker;
};
}

#endif
