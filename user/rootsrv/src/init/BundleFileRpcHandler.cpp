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

// set to log all IO
#define LOG_IO                          0

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
                case static_cast<uint32_t>(rpc::FileIoEpType::CloseFile):
                    if(!packet->replyPort) continue;
                    this->handleClose(msg, packet, err);
                    break;

                case static_cast<uint32_t>(rpc::FileIoEpType::ReadFileDirect):
                    if(!packet->replyPort) continue;
                    this->handleReadDirect(msg, packet, err);
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
    reply.maxReadBlockSize = kMaxBlockSize;

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
void BundleFileRpcHandler::openFailed(const int errno, const rpc::RpcPacket *packet) {
    rpc::FileIoOpenReply reply;
    reply.status = errno;

    auto replyBuf = cista::serialize(reply);
    this->reply(packet, rpc::FileIoEpType::OpenFileReply, replyBuf);
}

/**
 * Closes an open file handle.
 */
void BundleFileRpcHandler::handleClose(const struct MessageHeader *msg,
        const rpc::RpcPacket *packet, const size_t msgLen) {
    using namespace rpc;

    rpc::FileIoCloseReply reply;

    // deserialize the request
    auto data = std::span(packet->payload, msgLen - sizeof(RpcPacket));
    auto req = cista::deserialize<FileIoClose>(data);

    // ensure we've got such a file handle
    if(!this->openFiles.contains(req->file)) {
        reply.status = EBADF;
        auto replyBuf = cista::serialize(reply);
        return this->reply(packet, rpc::FileIoEpType::CloseFileReply, replyBuf);
    }

    // remove it and acknowledge removal
    this->openFiles.erase(req->file);

#if LOG_IO
    LOG("Closed file %u", req->file);
#endif

    reply.status = 0;
    auto replyBuf = cista::serialize(reply);
    this->reply(packet, rpc::FileIoEpType::CloseFileReply, replyBuf);
}
 


/**
 * Handles an open request.
 */
void BundleFileRpcHandler::handleReadDirect(const struct MessageHeader *msg,
        const rpc::RpcPacket *packet, const size_t msgLen) {
    using namespace rpc;
    rpc::FileIoReadReqReply reply;

    // deserialize the request and ensure length is ok
    auto data = std::span(packet->payload, msgLen - sizeof(RpcPacket));
    auto req = cista::deserialize<FileIoReadReq>(data);

    if(req->length > kMaxBlockSize) {
        return this->readFailed(req->file, EINVAL, packet);
    }

    // get the file
    if(!this->openFiles.contains(req->file)) {
        return this->readFailed(req->file, EBADF, packet);
    }
    reply.file = req->file;

    const auto &file = this->openFiles.at(req->file);

    // cap the read and offset values
    if(req->offset >= file.file->getSize()) {
        auto replyBuf = cista::serialize(reply);
        return this->reply(packet, rpc::FileIoEpType::ReadFileDirectReply, replyBuf);
    }

    auto offset = req->offset;
    auto length = std::min(req->length, (file.file->getSize() - offset));

    // perform the file read (memcopy)
#if LOG_IO
    LOG("Read req %x: off %llu len %llu", req->file, offset, length);
#endif
    auto range = file.file->getContents().subspan(offset, length);

    reply.data.resize(range.size());
    memcpy(reply.data.data(), range.data(), range.size());

    // send the reply
    auto replyBuf = cista::serialize(reply);

    this->reply(packet, rpc::FileIoEpType::ReadFileDirectReply, replyBuf);
}

/**
 * Sends a "read failed" message.
 */
void BundleFileRpcHandler::readFailed(const uintptr_t file, const int errno, const rpc::RpcPacket *packet) {
    rpc::FileIoReadReqReply reply;
    reply.status = errno;
    reply.file = file;

    auto replyBuf = cista::serialize(reply);
    this->reply(packet, rpc::FileIoEpType::ReadFileDirectReply, replyBuf);
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
