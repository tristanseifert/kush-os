#include "RpcHandler.h"
#include "Task.h"

#include <sys/syscalls.h>
#include <cista/serialization.h>

#include <cstring>
#include <span>

#include "log.h"
#include "dispensary/Dispensary.h"

#include "rpc/RootSrvTaskEndpoint.hpp"
#include <rpc/RpcPacket.hpp>

using namespace std::literals;
using namespace task;
using namespace rpc;

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
                case static_cast<uint32_t>(rpc::RootSrvTaskEpType::TaskCreate):
                    this->handleCreate(msg, packet);
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

/**
 * Processes the create task request.
 */
void RpcHandler::handleCreate(const struct MessageHeader *msg, const RpcPacket *packet) {
    // get at the data and try to deserialize
    auto reqBuf = std::span(packet->payload, msg->receivedBytes - sizeof(RpcPacket));
    auto req = cista::deserialize<RootSrvTaskCreate>(reqBuf);

    // create the task
    const std::string path(req->path);
    std::vector<std::string> params;

    for(const auto &arg : req->args) {
        params.emplace_back(arg);
    }

    auto taskHandle = Task::createFromFile(path, params);

    // build reply
    RootSrvTaskCreateReply reply;
    reply.handle = taskHandle;
    reply.status = (taskHandle ? 0 : -1);

    // send it
    auto buf = cista::serialize(reply);
    this->reply(packet, RootSrvTaskEpType::TaskCreateReply, buf);

}

/**
 * Sends an RPC message.
 */
void RpcHandler::reply(const rpc::RpcPacket *packet, const rpc::RootSrvTaskEpType type,
        const std::span<uint8_t> &buf) {
    int err;
    void *txBuf = nullptr;

    // allocate the reply buffer
    const auto replySize = buf.size() + sizeof(rpc::RpcPacket);
    err = posix_memalign(&txBuf, 16, replySize);
    if(err) {
        throw std::system_error(err, std::generic_category(), "posix_memalign");
    }

    auto txPacket = reinterpret_cast<rpc::RpcPacket *>(txBuf);
    txPacket->type = static_cast<uint32_t>(type);
    txPacket->replyPort = 0;

    memcpy(txPacket->payload, buf.data(), buf.size());

    // send it
    const auto replyPort = packet->replyPort;
    err = PortSend(replyPort, txPacket, replySize);

    free(txBuf);

    if(err) {
        throw std::system_error(err, std::generic_category(), "PortSend");
    }
}

