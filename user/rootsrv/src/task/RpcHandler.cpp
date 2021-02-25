#include "RpcHandler.h"

#include <sys/syscalls.h>
#include <cista/serialization.h>

#include <cstring>

#include "log.h"
#include "dispensary/Dispensary.h"

#include "rpc/RootSrvTaskEndpoint.hpp"
#include <rpc/RpcPacket.hpp>

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

    LOG("Task rpc port: $%08x'h", this->portHandle);

    // create the thread next
    this->run = true;
    this->worker = std::make_unique<std::thread>(&RpcHandler::main, this);

    // lastly, register the port
    dispensary::RegisterPort(kPortName, this->portHandle);
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
            // read out the type
            if(msg->receivedBytes < sizeof(rpc::RpcPacket)) {
                LOG("Port $%08x'h received too small message (%u)", this->portHandle,
                        msg->receivedBytes);
                continue;
            }

            const auto packet = reinterpret_cast<rpc::RpcPacket *>(msg->data);

            // invoke the appropriate handler
            switch(packet->type) {
                case static_cast<uint32_t>(rpc::RootSrvTaskEpType::kTaskCreate):
                    break;

                default:
                    LOG("Task RPC invalid msg type: $%08x", packet->type);
                    break;
            }
        }
        // an error occurred
        else {
            LOG("Port rx error: %d", err);
        }
    }

    // clean up
    free(rxBuf);
}
