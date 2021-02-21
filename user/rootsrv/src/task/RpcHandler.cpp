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
    int err;

    ThreadSetName(0, "rpc: task ep");

    // allocate receive buffers for messages
    void *rxBuf = nullptr;
    err = posix_memalign(&rxBuf, 16, kMaxMsgLen);
    REQUIRE(!err, "failed to allocate message rx buffer: %d", err);

    // process messages
    while(this->run) {
        // clear out any previous messages
        memset(rxBuf, 0, kMaxMsgLen);

        // read from the port
        struct MessageHeader *msg = (struct MessageHeader *) rxBuf;
        err = PortReceive(this->portHandle, msg, kMaxMsgLen, UINTPTR_MAX);

        if(err > 0) {

        }
        // an error occurred
        else {
            LOG("Port rx error: %d", err);
        }
    }

    // clean up
    free(rxBuf);
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
