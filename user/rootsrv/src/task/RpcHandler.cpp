#include "RpcHandler.h"
#include "Task.h"

#include <sys/syscalls.h>
#include <capnp/message.h>
#include <capnp/serialize.h>

#include <cstring>
#include <span>

#include "log.h"
#include "dispensary/Dispensary.h"

#include "rpc/TaskEndpoint.capnp.h"

#include <rpc/RpcPacket.hpp>
#include <rpc/Helpers.hpp>

using namespace std::literals;
using namespace task;
using namespace rpc;
using namespace rpc::TaskEndpoint;

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

    memset(rxBuf, 0, kMaxMsgLen);

    // process messages
    while(this->run) {
        // clear out any previous messages
        memset(rxBuf, 0, sizeof(struct MessageHeader));

        // read from the port
        struct MessageHeader *msg = (struct MessageHeader *) rxBuf;
        err = PortReceive(this->portHandle, msg, kMaxMsgLen, UINTPTR_MAX);

        if(err > 0) {
            // read out the type
            if(msg->receivedBytes < sizeof(rpc::RpcPacket)) {
                LOG("Port $%p'h received too small message (%u)", this->portHandle,
                        msg->receivedBytes);
                continue;
            }

            const auto packet = reinterpret_cast<rpc::RpcPacket *>(msg->data);

            // invoke the appropriate handler
            switch(packet->type) {
                case K_TYPE_CREATE_REQUEST:
                    this->handleCreate(msg, packet);
                    break;

                default:
                    LOG("Task RPC invalid msg type: $%0x", packet->type);
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

/**
 * Processes the create task request.
 */
void RpcHandler::handleCreate(const struct MessageHeader *msg, const RpcPacket *packet) {
    capnp::MallocMessageBuilder replyBuilder;
    auto reply = replyBuilder.initRoot<rpc::TaskEndpoint::CreateReply>();

    // get at the data and try to deserialize
    auto reqBuf = std::span(packet->payload, msg->receivedBytes - sizeof(RpcPacket));
    kj::ArrayPtr<const capnp::word> message(reinterpret_cast<const capnp::word *>(reqBuf.data()),
            reqBuf.size() / sizeof(capnp::word));
    capnp::FlatArrayMessageReader reader(message);
    auto req = reader.getRoot<rpc::TaskEndpoint::CreateRequest>();

    // create the task
    const std::string path(req.getPath());
    std::vector<std::string> params;

    for(const auto &arg : req.getArgs()) {
        params.emplace_back(arg);
    }

    try {
        auto taskHandle = Task::createFromFile(path, params);
        reply.setHandle(taskHandle);
        reply.setStatus(0);
    } catch(std::exception &e) {
        LOG("Failed to create task: %s", e.what());
        reply.setStatus(-1);
    }

    // send reply
    int err = RpcSend(packet->replyPort, K_TYPE_CREATE_REPLY, replyBuilder);
    if(err) {
        throw std::system_error(err, std::generic_category(), "RpcSend");
    }
}

