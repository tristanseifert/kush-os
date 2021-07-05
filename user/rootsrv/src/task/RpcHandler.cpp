#include "RpcHandler.h"
#include "Task.h"
#include "TaskEndpoint.h"

#include <sys/syscalls.h>
#include <mpack/mpack.h>

#include <cstring>
#include <span>

#include "log.h"
#include "dispensary/Dispensary.h"

#include <rpc/RpcPacket.hpp>
#include <rpc/Helpers.hpp>

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
                case static_cast<uint32_t>(TaskEndpointType::CreateTaskRequest):
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
    // deserialize the request
    auto reqBuf = std::span(packet->payload, msg->receivedBytes - sizeof(RpcPacket));
    mpack_tree_t tree;
    mpack_tree_init_data(&tree, reinterpret_cast<const char *>(reqBuf.data()), reqBuf.size());
    mpack_tree_parse(&tree);
    mpack_node_t root = mpack_tree_root(&tree);

    auto pathNode = mpack_node_map_cstr(root, "path");
    const std::string path(mpack_node_str(pathNode), mpack_node_strlen(pathNode));
    std::vector<std::string> params;

    auto argsNode = mpack_node_map_cstr(root, "args");
    if(!mpack_node_is_nil(argsNode)) {
        for(size_t i = 0; i < mpack_node_array_length(argsNode); i++) {
            auto arg = mpack_node_array_at(argsNode, i);
            params.emplace_back(mpack_node_str(arg), mpack_node_strlen(arg));
        }
    }

    // clean up the decoder and bail if error
    const auto readerStatus = mpack_tree_destroy(&tree);
    if(readerStatus != mpack_ok) {
        LOG("%s failed: %d", "mpack_tree_destroy", readerStatus);
        return;
    }

    // set up for building the reply...
    char *resData{nullptr};
    size_t resSize{0};

    mpack_writer_t writer;
    mpack_writer_init_growable(&writer, &resData, &resSize);

    mpack_start_map(&writer, 2);

    // ...create the task...
    try {
        auto taskHandle = Task::createFromFile(path, params);

        mpack_write_cstr(&writer, "status");
        mpack_write_i32(&writer, 1);
        mpack_write_cstr(&writer, "handle");
        mpack_write_u64(&writer, taskHandle);
    } catch(std::exception &e) {
        LOG("Failed to create task: %s", e.what());

        mpack_write_cstr(&writer, "status");
        mpack_write_i32(&writer, -1);
        mpack_write_cstr(&writer, "handle");
        mpack_write_nil(&writer);
    }

    // ...and send the reply.
    mpack_finish_map(&writer);

    const auto writerStatus = mpack_writer_destroy(&writer);
    if(writerStatus != mpack_ok) {
        LOG("%s failed: %d", "mpack_writer_destroy", writerStatus);
        return;
    }

    std::span<uint8_t> reply(reinterpret_cast<uint8_t *>(resData), resSize);
    int err = RpcSend(packet->replyPort, static_cast<uint32_t>(TaskEndpointType::CreateTaskReply),
            reply);
    if(err) {
        throw std::system_error(err, std::generic_category(), "RpcSend");
    }
}

