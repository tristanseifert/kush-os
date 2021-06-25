#include "LegacyIo.h"
#include "MessageLoop.h"

#include "Log.h"

#include <rpc/dispensary.h>
#include <rpc/RpcPacket.hpp>
#include <rpc/FileIO.hpp>
#include <sys/syscalls.h>

#include <cstdlib>
#include <cstring>

using namespace rpc;

/**
 * Initializes the worker.
 */
LegacyIo::LegacyIo(MessageLoop *_ml) : ml(_ml) {
    this->worker = std::make_unique<std::thread>(&LegacyIo::main, this);
}

/**
 * Cleans up the leagcy IO worker.
 */
LegacyIo::~LegacyIo() {
    // signal to terminate
    this->run = false;

    // send a dummy message
    uint32_t dummy{0};
    int err = PortSend(this->workerPort, &dummy, sizeof(dummy));
    if(err) {
        Trace("Failed to send legacy io shutdown message: %d", err);
    }
}

/**
 * Main loop for the legacy worker IO. This is a basic C struct based interface with a super low
 * overhead used by the dynamic linker and some early boot services that don't have the full system
 * set up yet.
 */
void LegacyIo::main() {
    int err;

    ThreadSetName(0, "Legacy file io rpc");

    // set up
    err = PortCreate(&this->workerPort);
    if(err) {
        Abort("%s failed: %d", "PortCreate", err);
    }

    // allocate buffers
    void *rxBuf{nullptr};
    err = posix_memalign(&rxBuf, 16, kMaxMsgLen);
    if(err) {
        Abort("%s failed: %d", "posix_memalign", err);
    }

    // register port
    err = RegisterService(kServiceName.data(), this->workerPort);
    if(err) {
        Abort("%s failed: %d", "RegisterService", err);
    }

    Trace("Legacy worker set up: port $%p'h", this->workerPort);

    // run loop
    while(this->run) {
        // clear out any previous messages
        memset(rxBuf, 0, kMaxMsgLen);

        // read from the port
        struct ::MessageHeader *msg = (struct ::MessageHeader *) rxBuf;
        err = PortReceive(this->workerPort, msg, kMaxMsgLen, UINTPTR_MAX);

        if(err > 0) {
            // read out the type
            if(msg->receivedBytes < sizeof(RpcPacket)) {
                Trace("Legacy io port received too small message (%u)", msg->receivedBytes);
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

                default:
                    Warn("Legacy io invalid msg type: $%08x", packet->type);
                    break;
            }
        }
        // an error occurred
        else {
            Warn("Legacy io port rx error: %d", err);
        }
    }

    // clean up
    Trace("Legacy worker exiting");

    PortDestroy(this->workerPort);
    free(rxBuf);
}


/**
 * Handles a "get capabilities" request.
 */
void LegacyIo::handleGetCaps(const struct MessageHeader *msg, const RpcPacket *packet,
        const size_t msgLen) {
    // send the reply
    FileIoGetCapsReply reply;
    reply.version = 1;
    reply.capabilities = FileIoCaps::DirectIo;
    reply.maxReadBlockSize = kMaxBlockSize;

    auto buf = std::span<uint8_t>(reinterpret_cast<uint8_t *>(&reply), sizeof(reply));
    this->reply(packet, FileIoEpType::GetCapabilitiesReply, buf);
}


/**
 * Sends an RPC message.
 */
void LegacyIo::reply(const RpcPacket *packet, const FileIoEpType type,
        const std::span<uint8_t> &buf) {
    int err;
    void *txBuf{nullptr};

    // allocate the reply buffer
    const auto replySize = buf.size() + sizeof(RpcPacket);
    err = posix_memalign(&txBuf, 16, replySize + 16);
    if(err) {
        Abort("%s failed: %d", "posix_memalign", err);
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
        Warn("%s failed: %d", "PortSend", err);
    }
}

/**
 * Handles an open request.
 */
void LegacyIo::handleOpen(const struct MessageHeader *msg, const RpcPacket *packet,
        const size_t msgLen) {
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

    // attempt to open the file and send a reply
    auto ret = this->ml->implOpenFile(path, 0);

    FileIoOpenReply reply;
    reply.status = ret.status;
    reply.flags = req->mode;
    reply.fileHandle = ret.handle;
    reply.length = ret.fileSize;

    auto replyBuf = std::span<uint8_t>(reinterpret_cast<uint8_t *>(&reply), sizeof(reply));
    this->reply(packet, FileIoEpType::OpenFileReply, replyBuf);
}

/**
 * Sends a "file open failed" message as a response to a previous open request.
 */
void LegacyIo::openFailed(const int errno, const RpcPacket *packet) {
    FileIoOpenReply reply;
    memset(&reply, 0, sizeof(reply));

    reply.status = errno;

    auto replyBuf = std::span<uint8_t>(reinterpret_cast<uint8_t *>(&reply), sizeof(reply));
    this->reply(packet, FileIoEpType::OpenFileReply, replyBuf);
}

/**
 * Closes an open file handle.
 */
void LegacyIo::handleClose(const struct MessageHeader *msg, const RpcPacket *packet,
        const size_t msgLen) {
    FileIoCloseReply reply;

    // deserialize the request
    auto data = std::span(packet->payload, msgLen - sizeof(RpcPacket));
    if(data.size() < sizeof(reply)) {
        reply.status = EINVAL;
        auto replyBuf = std::span<uint8_t>(reinterpret_cast<uint8_t *>(&reply), sizeof(reply));
        return this->reply(packet, FileIoEpType::CloseFileReply, replyBuf);
    }
    auto req = reinterpret_cast<const FileIoClose *>(data.data());

    // forward request
    reply.status = this->ml->implCloseFile(req->file);

    auto replyBuf = std::span<uint8_t>(reinterpret_cast<uint8_t *>(&reply), sizeof(reply));
    this->reply(packet, FileIoEpType::CloseFileReply, replyBuf);
}

/**
 * Handles an open request.
 */
void LegacyIo::handleReadDirect(const struct MessageHeader *msg, const RpcPacket *packet,
        const size_t msgLen) {
    int err;

    // deserialize the request and ensure length is ok
    auto reqData = std::span(packet->payload, msgLen - sizeof(RpcPacket));
    if(reqData.size() < sizeof(FileIoReadReq)) {
        // XXX: can we get the file handle?
        return this->readFailed(0, EINVAL, packet);
    }

    auto req = reinterpret_cast<const FileIoReadReq *>(reqData.data());

    if(req->length > kMaxBlockSize) {
        return this->readFailed(req->file, EINVAL, packet);
    }

    // forward request
    auto ret = this->ml->implSlowRead(req->file, req->offset, req->length);

    if(ret.status) {
        return this->readFailed(req->file, ret.status, packet);
    }

    // fill out the reply buffer
    const auto &data = ret.data;
    this->ensureReadReplyBufferSize(data.size());

    auto txPacket = reinterpret_cast<RpcPacket *>(this->readReplyBuffer);
    auto reply = reinterpret_cast<FileIoReadReqReply *>(txPacket->payload);

    memset(txPacket, 0, sizeof(RpcPacket));
    memset(reply, 0, sizeof(FileIoReadReqReply));

    reply->status = 0;
    reply->file = req->file;
    reply->dataLen = data.size();
    memcpy(reply->data, data.data(), data.size());

    txPacket->type = static_cast<uint32_t>(FileIoEpType::ReadFileDirectReply);
    txPacket->replyPort = 0;

    // send it
    const auto replyPort = packet->replyPort;
    const size_t replySize = sizeof(RpcPacket) + sizeof(FileIoReadReqReply) + data.size();
    err = PortSend(replyPort, txPacket, replySize);

    if(err) {
        Warn("%s failed: %d", "PortSend", err);
    }
}

/**
 * Ensures the reply buffer is at least large enough to handle a message with a payload of the
 * given size.
 */
void LegacyIo::ensureReadReplyBufferSize(const size_t payloadBytes) {
    // calculate total size required and bail if we've got this much
    const size_t total = sizeof(RpcPacket) + sizeof(FileIoReadReqReply) + 16 + payloadBytes;
    if(this->readReplyBufferSize >= total) return;

    // release old buffer and allocate new
    if(this->readReplyBuffer) {
        free(this->readReplyBuffer);
    }

    int err = posix_memalign(&this->readReplyBuffer, 16, total);
    if(err) {
        Abort("%s failed: %d", "posix_memalign", err);
    }
    this->readReplyBufferSize = total;
}

/**
 * Sends a "read failed" message.
 */
void LegacyIo::readFailed(const uintptr_t file, const int errno, const RpcPacket *packet) {
    FileIoReadReqReply reply;
    memset(&reply, 0, sizeof(reply));

    reply.status = errno;
    reply.file = file;

    auto replyBuf = std::span<uint8_t>(reinterpret_cast<uint8_t *>(&reply), sizeof(reply));
    this->reply(packet, FileIoEpType::ReadFileDirectReply, replyBuf);
}
