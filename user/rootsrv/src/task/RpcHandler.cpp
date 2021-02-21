#include "RpcHandler.h"

#include <sys/syscalls.h>
#include <cista/serialization.h>

#include <cstring>

#include "log.h"
#include "dispensary/Dispensary.h"
#include "rpc/TaskEndpoint.hpp"

using namespace std::literals;
using namespace task;

// declare the constants for the handler
const std::string_view RpcHandler::kPortName = "me.blraaz.rpc.rootsrv.task"sv;

/// Shared RPC handler instance
RpcHandler *RpcHandler::gShared = nullptr;

/**
 * Initializes the RPC handler. This sets up the listening port, the worker thread, and then
 * registers the service.
 */
RpcHandler::RpcHandler() {
    int err;

    // set up the port
    err = PortCreate(&this->portHandle);
    REQUIRE(!err, "failed to create task rpc port: %d", err);

    // create the thread next
    this->run = true;
    this->worker = std::make_unique<std::thread>(&RpcHandler::main, this);

    auto native = this->worker->native_handle();
    LOG("thanks for the threadule! %p %p $%08x'h", this->worker.get(), native,
            thrd_get_handle_np(native));

    // lastly, register the port
    // dispensary::RegisterPort(kPortName, this->port);
}

/**
 * Entry point for the task RPC handler thread.
 *
 * This continuously reads from the port, waiting to receive a request.
 */
void RpcHandler::main() {
    ThreadSetName(0, "rpc: task ep");

    LOG("we chillin boi");

    // process messages
    while(this->run) {
        std::this_thread::yield();
    }

    // clean up
    LOG("oh no");
}

/**
 * Handles a received message.
 *
 * @return Whether the message was handled.
 */
bool RpcHandler::handleMsg(const MessageHeader_t *msg, const size_t len) {
    uint32_t type;

    // read the first 4 bytes of the message to get the message type
    if(len < sizeof(uint32_t)) return false;
    memcpy(&type, msg->data, sizeof(uint32_t));

    // invoke the handler based on its type
    switch(type) {
        // task creation request
        case static_cast<uint32_t>(rpc::RootSrvTaskEpType::kTaskCreate):
            return this->handleTaskCreate(msg, len);

        // unhandled message type
        default:
            LOG("Unhandled task rpc message: $%08x", type);
            break;
    }

    // if we get here, the message was not handled
    return false;
}
