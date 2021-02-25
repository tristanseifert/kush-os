#include "RpcHandler.h"
#include "Registry.h"
#include "Dispensary.h"

#include "task/InfoPage.h"

#include <sys/_infopage.h>
#include <sys/syscalls.h>
#include <rpc/RpcPacket.hpp>
#include <rpc/RootSrvDispensaryEndpoint.hpp>

#include <malloc.h>
#include <cstring>
#include <span>
#include <system_error>
#include <vector>

#include "log.h"

using namespace dispensary;
using namespace rpc;

/// shared rpc handler instance
RpcHandler *RpcHandler::gShared = nullptr;

/**
 * Initializes the dispensary RPC handler.
 */
RpcHandler::RpcHandler() {
    int err;

    // set up the port
    err = PortCreate(&this->portHandle);
    if(err) {
        throw std::system_error(err, std::generic_category(), "PortCreate");
    }

    task::InfoPage::gShared->info->dispensaryPort = this->portHandle;

    LOG("Dispensary port: $%08x'h", this->portHandle);

    // create the thread next
    this->run = true;
    this->worker = std::make_unique<std::thread>(&RpcHandler::main, this);
}

/**
 * Entry point for the dispensary RPC handler.
 */
void RpcHandler::main() {
    int err;

    ThreadSetName(0, "rpc: dispensary ep");

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
            if(msg->receivedBytes < sizeof(RpcPacket)) {
                LOG("Port $%08x'h received too small message (%u)", this->portHandle,
                        msg->receivedBytes);
                continue;
            }

            const auto packet = reinterpret_cast<RpcPacket *>(msg->data);

            // invoke the appropriate handler
            switch(packet->type) {
                case static_cast<uint32_t>(RootSrvDispensaryEpType::Lookup):
                    if(!packet->replyPort) continue;
                    this->handleLookup(msg, packet, err);
                    break;

                case static_cast<uint32_t>(RootSrvDispensaryEpType::Register):
                    if(!packet->replyPort) continue;
                    this->handleRegister(msg, packet, err);
                    break;

                default:
                    LOG("Dispensary RPC invalid msg type: $%08x", packet->type);
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
 * Handles a lookup request.
 */
void RpcHandler::handleLookup(const struct MessageHeader *msg, const rpc::RpcPacket *packet,
        const size_t msgLen) {
    // deserialize the request
    auto data = std::span(packet->payload, msgLen - sizeof(RpcPacket));
    if(data.size() < sizeof(RootSrvDispensaryLookup)) {
        return;
    }

    auto req = reinterpret_cast<const RootSrvDispensaryLookup *>(data.data());
    if(req->nameLen > (data.size() - sizeof(RootSrvDispensaryLookup))) {
        return;
    }

    const std::string name(req->name, req->nameLen);

    // perform lookup
    uintptr_t handle = 0;
    bool found = Registry::gShared->lookupPort(name, handle);

    LOG("Request for port '%s': resolved %d ($%08x'h)", name.c_str(), found, handle);

    // build response
    std::vector<uint8_t> replyBuf;
    replyBuf.resize(sizeof(RootSrvDispensaryLookupReply) + req->nameLen + 1, 0);

    auto reply = reinterpret_cast<RootSrvDispensaryLookupReply *>(replyBuf.data());

    reply->status = found ? 0 : 1;
    reply->port = handle;
    reply->nameLen = req->nameLen;
    memcpy(reply->name, req->name, req->nameLen);

    // send it
    this->reply(packet, RootSrvDispensaryEpType::LookupReply, replyBuf);
}

/**
 * Handles a registration request.
 *
 * Currently, all tasks are allowed to overwrite registrations from any other task. In the future,
 * we should add some scoping to this.
 */
void RpcHandler::handleRegister(const struct MessageHeader *msg, const rpc::RpcPacket *packet,
        const size_t msgLen) {
    // deserialize request and register it
    auto data = std::span(packet->payload, msgLen - sizeof(RpcPacket));
    if(data.size() < sizeof(RootSrvDispensaryRegister)) {
        return;
    }

    auto req = reinterpret_cast<const RootSrvDispensaryRegister *>(data.data());
    if(req->nameLen > (data.size() - sizeof(RootSrvDispensaryRegister))) {
        return;
    }

    const std::string name(req->name, req->nameLen);

    bool replaced = Registry::gShared->registerPort(name, req->portHandle);

    // build response
    std::vector<uint8_t> replyBuf;
    replyBuf.resize(sizeof(RootSrvDispensaryLookupReply) + req->nameLen + 1, 0);

    auto reply = reinterpret_cast<RootSrvDispensaryRegisterReply *>(replyBuf.data());

    reply->status = 0;
    reply->replaced = replaced;
    reply->nameLen = req->nameLen;
    memcpy(reply->name, req->name, req->nameLen);

    // send it
    this->reply(packet, RootSrvDispensaryEpType::RegisterReply, replyBuf);
}

/**
 * Sends an RPC message.
 */
void RpcHandler::reply(const rpc::RpcPacket *packet, const rpc::RootSrvDispensaryEpType type,
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
