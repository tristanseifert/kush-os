#include "BundleFileRpcHandler.h"
#include "Bundle.h"

#include "log.h"
#include "dispensary/Dispensary.h"

#include <sys/syscalls.h>

#include <rpc/RpcPacket.hpp>
#include <rpc/FileIO.hpp>

#include <cerrno>
#include <system_error>
#include <span>
#include <vector>

using namespace std::literals;
using namespace init;
using namespace rpc;

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

    LOG("Bundle file IO port: $%p'h", this->portHandle);

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
            if(msg->receivedBytes < sizeof(RpcPacket)) {
                LOG("Init file io received too small message (%lu)", msg->receivedBytes);
                continue;
            }

            const auto packet = reinterpret_cast<RpcPacket *>(msg->data);

            // invoke the appropriate handler
            switch(packet->type) {
                case static_cast<uint32_t>(FileIoEpType::GetCapabilities):
                    if(!packet->replyPort) continue;
                    this->handleGetCaps(msg, packet, err);
                    break;

                case static_cast<uint32_t>(FileIoEpType::OpenFile):
                    if(!packet->replyPort) continue;
                    this->handleOpen(msg, packet, err);
                    break;
                case static_cast<uint32_t>(FileIoEpType::CloseFile):
                    if(!packet->replyPort) continue;
                    this->handleClose(msg, packet, err);
                    break;

                case static_cast<uint32_t>(FileIoEpType::ReadFileDirect):
                    if(!packet->replyPort) continue;
                    this->handleReadDirect(msg, packet, err);
                    break;

                case kShutdownMessage:
                    this->shutdown();
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
    LOG("Init file io service shutting down");
}

/**
 * Handles a "get capabilities" request.
 */
void BundleFileRpcHandler::handleGetCaps(const struct MessageHeader *msg,
        const RpcPacket *packet, const size_t msgLen) {
    // send the reply
    FileIoGetCapsReply reply;
    reply.version = 1;
    reply.capabilities = FileIoCaps::DirectIo;
    reply.maxReadBlockSize = kMaxBlockSize;

    auto buf = std::span<uint8_t>(reinterpret_cast<uint8_t *>(&reply), sizeof(reply));
    this->reply(packet, FileIoEpType::GetCapabilitiesReply, buf);
}


/**
 * Handles an open request.
 */
void BundleFileRpcHandler::handleOpen(const struct MessageHeader *msg,
        const RpcPacket *packet, const size_t msgLen) {
    // deserialize the request
    auto data = std::span(packet->payload, msgLen - sizeof(RpcPacket));
    if(data.size() < sizeof(FileIoOpen)) {
        return this->openFailed(EINVAL, packet);
    }

    auto req = reinterpret_cast<const FileIoOpen *>(data.data());
    if(req->pathLen > data.size() - sizeof(FileIoOpen)) {
        // packet too small to hold string
        return this->openFailed(EINVAL, packet);
    }

    const std::string path(req->path, req->pathLen);

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

    auto replyBuf = std::span<uint8_t>(reinterpret_cast<uint8_t *>(&reply), sizeof(reply));
    this->reply(packet, FileIoEpType::OpenFileReply, replyBuf);
}

/**
 * Sends a "file open failed" message as a response to a previous open request.
 */
void BundleFileRpcHandler::openFailed(const int errno, const RpcPacket *packet) {
    FileIoOpenReply reply;
    memset(&reply, 0, sizeof(reply));

    reply.status = errno;

    auto replyBuf = std::span<uint8_t>(reinterpret_cast<uint8_t *>(&reply), sizeof(reply));
    this->reply(packet, FileIoEpType::OpenFileReply, replyBuf);
}

/**
 * Closes an open file handle.
 */
void BundleFileRpcHandler::handleClose(const struct MessageHeader *msg,
        const RpcPacket *packet, const size_t msgLen) {
    FileIoCloseReply reply;

    // deserialize the request
    auto data = std::span(packet->payload, msgLen - sizeof(RpcPacket));
    if(data.size() < sizeof(reply)) {
        reply.status = EINVAL;
        auto replyBuf = std::span<uint8_t>(reinterpret_cast<uint8_t *>(&reply), sizeof(reply));
        return this->reply(packet, FileIoEpType::CloseFileReply, replyBuf);
    }
    auto req = reinterpret_cast<const FileIoClose *>(data.data());

    // ensure we've got such a file handle
    if(!this->openFiles.contains(req->file)) {
        reply.status = EBADF;
        auto replyBuf = std::span<uint8_t>(reinterpret_cast<uint8_t *>(&reply), sizeof(reply));
        return this->reply(packet, FileIoEpType::CloseFileReply, replyBuf);
    }

    // remove it and acknowledge removal
    this->openFiles.erase(req->file);

#if LOG_IO
    LOG("Closed file %u", req->file);
#endif

    reply.status = 0;
    auto replyBuf = std::span<uint8_t>(reinterpret_cast<uint8_t *>(&reply), sizeof(reply));
    this->reply(packet, FileIoEpType::CloseFileReply, replyBuf);
}
 


/**
 * Handles an open request.
 */
void BundleFileRpcHandler::handleReadDirect(const struct MessageHeader *msg,
        const RpcPacket *packet, const size_t msgLen) {
    // deserialize the request and ensure length is ok
    auto data = std::span(packet->payload, msgLen - sizeof(RpcPacket));
    if(data.size() < sizeof(FileIoReadReq)) {
        // XXX: can we get the file handle?
        return this->readFailed(0, EINVAL, packet);
    }

    auto req = reinterpret_cast<const FileIoReadReq *>(data.data());

    if(req->length > kMaxBlockSize) {
        return this->readFailed(req->file, EINVAL, packet);
    }

    // get the file
    if(!this->openFiles.contains(req->file)) {
        return this->readFailed(req->file, EBADF, packet);
    }

    const auto &file = this->openFiles.at(req->file);

    // cap the read and offset values
    if(req->offset >= file.file->getSize()) {
        return this->readFailed(req->file, EINVAL, packet);
    }

    auto offset = req->offset;
    auto length = std::min(req->length, (file.file->getSize() - offset));


    // perform the file read (memcopy)
#if LOG_IO
    LOG("Read req %x: off %llu len %llu", req->file, offset, length);
#endif
    auto range = file.file->getContents().subspan(offset, length);

    // allocate the reply buffer and fill it out
    std::vector<uint8_t> replyBuf;
    replyBuf.resize(sizeof(FileIoReadReqReply) + range.size(), 0);
    auto reply = reinterpret_cast<FileIoReadReqReply *>(replyBuf.data());

    reply->status = 0;
    reply->file = req->file;
    reply->dataLen = range.size();
    memcpy(reply->data, range.data(), range.size());

    this->reply(packet, FileIoEpType::ReadFileDirectReply, replyBuf);
}

/**
 * Sends a "read failed" message.
 */
void BundleFileRpcHandler::readFailed(const uintptr_t file, const int errno, const RpcPacket *packet) {
    FileIoReadReqReply reply;
    memset(&reply, 0, sizeof(reply));

    reply.status = errno;
    reply.file = file;

    auto replyBuf = std::span<uint8_t>(reinterpret_cast<uint8_t *>(&reply), sizeof(reply));
    this->reply(packet, FileIoEpType::ReadFileDirectReply, replyBuf);
}

/**
 * Sends an RPC message.
 */
void BundleFileRpcHandler::reply(const RpcPacket *packet, const FileIoEpType type,
        const std::span<uint8_t> &buf) {
    int err;
    void *txBuf = nullptr;

    // allocate the reply buffer
    const auto replySize = buf.size() + sizeof(RpcPacket);
    err = posix_memalign(&txBuf, 16, replySize);
    if(err) {
        throw std::system_error(err, std::generic_category(), "posix_memalign");
    }

    auto txPacket = reinterpret_cast<RpcPacket *>(txBuf);
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

/**
 * Clean up and shut down the handler.
 */
extern "C" void __librpc__FileIoResetConnection();

void BundleFileRpcHandler::shutdown() {
    // unregister port and deallocate it
    dispensary::UnregisterPort(kPortName);
    PortDestroy(this->portHandle);

    // ensure worker exits
    this->run = false;

    // make future file IO use the filesystem
    __librpc__FileIoResetConnection();
}
