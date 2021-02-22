#include "BundleFileRpcHandler.h"
#include "Bundle.h"

#include "log.h"
#include "dispensary/Dispensary.h"

#include <sys/syscalls.h>

#include <rpc/RpcPacket.hpp>
#include <rpc/FileIO.hpp>
#include <cista/serialization.h>

#include <cerrno>
#include <system_error>
#include <span>

using namespace std::literals;
using namespace init;

// declare the constants for the handler
const std::string_view BundleFileRpcHandler::kPortName = "me.blraaz.rpc.rootsrv.initfileio"sv;
// shared instance of the bundle file IO handler
BundleFileRpcHandler *BundleFileRpcHandler::gShared = nullptr;

/**
 * Initializes the bundle RPC handler. We'll allocate a port and start the work loop.
 */
BundleFileRpcHandler::BundleFileRpcHandler(std::shared_ptr<Bundle> &_bundle) : bundle(_bundle) {
    int err;

    // set up the port
    err = PortCreate(&this->portHandle);
    if(err) {
        throw std::system_error(err, std::generic_category(), "PortCreate");
    }

    LOG("Bundle file IO port: $%08x'h", this->portHandle);

    // create the thread next
    this->run = true;
    this->worker = std::make_unique<std::thread>(&BundleFileRpcHandler::main, this);

    // lastly, register the port
    dispensary::RegisterPort(kPortName, this->portHandle);
}

/**
 * Main loop for the init bundle file RPC service
 */
void BundleFileRpcHandler::main() {
    int err;

    ThreadSetName(0, "rpc: init file ep");

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
                case static_cast<uint32_t>(rpc::FileIoEpType::GetCapabilities):
                    if(!packet->replyPort) continue;
                    this->handleGetCaps(msg, packet, err);
                    break;

                case static_cast<uint32_t>(rpc::FileIoEpType::OpenFile):
                    if(!packet->replyPort) continue;
                    this->handleOpen(msg, packet, err);
                    break;

                default:
                    LOG("Init file RPC invalid msg type: $%08x", packet->type);
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
 * Handles a "get capabilities" request.
 */
void BundleFileRpcHandler::handleGetCaps(const struct MessageHeader *msg,
        const rpc::RpcPacket *packet, const size_t msgLen) {
    using namespace rpc;

    // deserialize the request
    // auto data = std::span(packet->payload, msgLen - sizeof(RpcPacket));
    // auto req = cista::deserialize<FileIoGetCaps>(data);

    // send the reply
    FileIoGetCapsReply reply;
    reply.version = 1;
    reply.capabilities = FileIoCaps::DirectIo;

    auto replyBuf = cista::serialize(reply);
    this->reply(packet, FileIoEpType::GetCapabilitiesReply, replyBuf);
}


/**
 * Handles an open request.
 */
void BundleFileRpcHandler::handleOpen(const struct MessageHeader *msg,
        const rpc::RpcPacket *packet, const size_t msgLen) {
    using namespace rpc;

    // deserialize the request
    auto data = std::span(packet->payload, msgLen - sizeof(RpcPacket));
    auto req = cista::deserialize<FileIoOpen>(data);
    const std::string path(req->path);

    // file may only be opened read-only
    if(static_cast<uint32_t>(req->mode & FileIoOpenFlags::WriteOnly)) {
        return this->openFailed(EROFS, packet);
    }

    // attempt to open the file
    auto file = this->bundle->open(path);
    if(!file) {
        return this->openFailed(ENOENT, packet);
    }

    // store an info structure
    const auto handle = this->nextHandle++;
    this->openFiles.emplace(handle, OpenedFile(file, msg->senderTask, msg->senderThread));

    // send the reply
    FileIoOpenReply reply;
    reply.status = 0;
    reply.flags = req->mode;
    reply.fileHandle = handle;
    reply.length = file->getSize();

    auto replyBuf = cista::serialize(reply);
    this->reply(packet, FileIoEpType::OpenFileReply, replyBuf);
}

/**
 * Sends a "file open failed" message as a response to a previous open request.
 */
void BundleFileRpcHandler::openFailed(int errno, const rpc::RpcPacket *packet) {
    rpc::FileIoOpenReply reply;
    reply.status = errno;

    auto replyBuf = cista::serialize(reply);
    this->reply(packet, rpc::FileIoEpType::OpenFileReply, replyBuf);
}



/**
 * Sends an RPC message.
 */
void BundleFileRpcHandler::reply(const rpc::RpcPacket *packet, const rpc::FileIoEpType type,
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
