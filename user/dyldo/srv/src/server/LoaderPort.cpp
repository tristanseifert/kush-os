#include "LoaderPort.h"

#include <malloc.h>

#include <cassert>
#include <cstring>
#include <span>

#include <cista/serialization.h>
#include <sys/syscalls.h>
#include <rpc/dispensary.h>
#include <rpc/RpcPacket.hpp>
#include "rpc/LoaderPort.h"

#include "prelink/Prelink.h"

#include "log.h"

using namespace server;
using namespace rpc;

LoaderPort *LoaderPort::gShared = nullptr;
const std::string LoaderPort::kPortName = "me.blraaz.rpc.rt.dyld.loader";

/**
 * Initializes the shared loader port instance.
 */
void LoaderPort::init() {
    assert(!gShared);
    gShared = new LoaderPort;
}

/**
 * Allocates the port and work thread for the loader port.
 */
LoaderPort::LoaderPort() {
    int err;

    // set up the port
    err = PortCreate(&this->port);
    if(err) {
        L("Failed to create port: {}", err);
        std::terminate();
    }

    L("LoaderPort: {}", this->port);

    // create worker thread
    this->run = true;
    this->worker = std::make_unique<std::thread>(&LoaderPort::main, this);

    // register the port
    err = RegisterService(kPortName.c_str(), this->port);
    if(err) {
        L("Failed to register port: {}", err);
        std::terminate();
    }
}

/**
 * Main message processing loop
 */
void LoaderPort::main() {
    int err;

    ThreadSetName(0, "rpc: loader port");

    // set up our receive buffer
    void *rxBuf = nullptr;
    err = posix_memalign(&rxBuf, 16, kMsgBufLen);
    if(err) {
        L("Failed to allocate loader port rx buf: {}", err);
        std::terminate();
    }

    // process messages
    while(this->run) {
        // clear out any previous messages
        memset(rxBuf, 0, kMsgBufLen);

        // read from the port
        struct MessageHeader *msg = (struct MessageHeader *) rxBuf;
        err = PortReceive(this->port, msg, kMsgBufLen, UINTPTR_MAX);

        if(err > 0) {
            // read out the type
            if(msg->receivedBytes < sizeof(RpcPacket)) {
                L("Port $%08x'h received too small message (%u)", this->port, msg->receivedBytes);
                continue;
            }

            const auto packet = reinterpret_cast<RpcPacket *>(msg->data);

            // invoke the appropriate handler
            switch(packet->type) {
                case static_cast<uint32_t>(DyldoLoaderEpType::TaskCreated):
                    if(!packet->replyPort) continue;
                    this->handleTaskCreated(msg, packet);
                    break;

                default:
                    L("LoaderPort RPC invalid msg type: $%08x", packet->type);
                    break;
            }
        }
        // an error occurred
        else {
            L("Port rx error: %d", err);
        }
    }

    // clean up
    free(rxBuf);
}

/**
 * Handles a newly created task.
 */
void LoaderPort::handleTaskCreated(const struct MessageHeader *msg, const rpc::RpcPacket *packet) {
    uintptr_t pc = 0;

    // deserialize the request
    auto data = std::span(packet->payload, msg->receivedBytes - sizeof(RpcPacket));
    auto req = cista::deserialize<DyldoLoaderTaskCreated>(data);
    const std::string path(req->binaryPath);

    L("New task ${:08x}'h (path '{}')", req->taskHandle, path);

    // set up prelink binaries and get dynamic linker entry
    prelink::BootstrapTask(req->taskHandle, pc);

    // build response
    DyldoLoaderTaskCreatedReply reply;
    reply.taskHandle = req->taskHandle;

    reply.status = 0;
    reply.entryPoint = pc;

    // serialize it and stick it in an RPC packet
    auto replyBuf = cista::serialize(reply);
    this->reply(packet, DyldoLoaderEpType::TaskCreatedReply, replyBuf);
}

/**
 * Sends an RPC message.
 */
void LoaderPort::reply(const rpc::RpcPacket *packet, const rpc::DyldoLoaderEpType type,
        const std::span<uint8_t> &buf) {
    int err;
    void *txBuf = nullptr;

    // allocate the reply buffer
    const auto replySize = buf.size() + sizeof(rpc::RpcPacket);
    err = posix_memalign(&txBuf, 16, replySize);
    if(err) {
        L("Failed to allocate loader port tx buf: {}", err);
        std::terminate();
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
        L("Failed to send to port: {}", err);
        std::terminate();
    }
}
