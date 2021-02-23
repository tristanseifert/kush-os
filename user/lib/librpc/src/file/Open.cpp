#include "FileIo.h"

#include "rpc/dispensary.h"
#include "rpc/file.h"
#include "sys/infopage.h"
#include "sys/syscalls.h"

#include "helpers/Send.h"

#include <malloc.h>
#include <threads.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <span>
#include <vector>

#include <cista/serialization.h>
#include <rpc/RpcPacket.hpp>
#include <rpc/FileIO.hpp>

using namespace rpc;
using namespace fileio;

/**
 * Opens a file by name.
 */
int FileOpen(const char * _Nonnull _path, const uintptr_t flags, uintptr_t * _Nonnull outHandle,
        uint64_t *outLength) {
    std::vector<uint8_t> requestBuf;
    int err;
    void *rxBuf = nullptr;
    struct MessageHeader *rxMsg = nullptr;
    FileIoOpen open;

    // validate args
    if(!_path || !outHandle) return -1;

    const std::string path(_path);

    // perform one-time init if needed
    call_once(&gStateOnceFlag, []{
        Init();
    });

    // acquire lock
    err = mtx_lock(&gState.lock);
    if(err != thrd_success) return -1;

    // perform the IO service lookup
    if(!gState.ioServerPort) {
        if(!UpdateServerPort()) {
            err = -2;
            goto fail;
        }
    }

    // send the request
    open.path = path;

    if(flags & FILE_OPEN_READ) {
        open.mode |= FileIoOpenFlags::ReadOnly;
    }
    if(flags & FILE_OPEN_WRITE) {
        open.mode |= FileIoOpenFlags::WriteOnly;
    }

    requestBuf = cista::serialize(open);

    err = _RpcSend(gState.ioServerPort, static_cast<uint32_t>(FileIoEpType::OpenFile),
            requestBuf, gState.replyPort);
    if(err) {
        err = -3;
        goto fail;
    }

    // allocate a receive buffer
    constexpr static const size_t kReplyBufSize = 256 + sizeof(struct MessageHeader);
    err = posix_memalign(&rxBuf, 16, kReplyBufSize);
    if(err) {
        goto fail;
    }

    // receive pls
    rxMsg = (struct MessageHeader *) rxBuf;
    err = PortReceive(gState.replyPort, rxMsg, kReplyBufSize, UINTPTR_MAX);

    mtx_unlock(&gState.lock);

    if(err > 0) {
        // read out the type
        if(rxMsg->receivedBytes < sizeof(RpcPacket)) {
            err = -4;
            goto fail;
        }

        const auto packet = reinterpret_cast<RpcPacket *>(rxMsg->data);
        if(packet->type != static_cast<uint32_t>(FileIoEpType::OpenFileReply)) {
            fprintf(stderr, "%s received wrong packet type %08x!\n", __PRETTY_FUNCTION__, packet->type);
            err = -4;
            goto fail;
        }

        // deserialize the capabilities response
        auto data = std::span(packet->payload, err - sizeof(RpcPacket));
        auto req = cista::deserialize<FileIoOpenReply>(data);

        if(!req->status) {
            *outHandle = req->fileHandle;

            if(outLength) {
                *outLength = req->length;
            }
        }
        // failed to open the file
        else {
            err = -5;
            goto fail;
        }
    } 
    // message too short for even the header
    else {
        err = -4;
        goto fail;
    }

    free(rxBuf);
    return 0;

fail:;
    // failure case: ensures the lock is released
    if(rxBuf) free(rxBuf);

    mtx_unlock(&gState.lock);
    return err;
}

/*
 * Closes a previously opened file
 */
int FileClose(const uintptr_t file) {
    int err;
    std::vector<uint8_t> requestBuf;
    void *rxBuf = nullptr;
    struct MessageHeader *rxMsg = nullptr;

    // allocate a receive buffer
    constexpr static const size_t kReplyBufSize = 256 + sizeof(struct MessageHeader);
    err = posix_memalign(&rxBuf, 16, kReplyBufSize);
    if(err) {
        goto fail;
    }

    // acquire lock
    err = mtx_lock(&gState.lock);
    if(err != thrd_success) {
        free(rxBuf);
        return -1;
    }

    // send the request
    FileIoClose req;
    req.file = file;

    requestBuf = cista::serialize(req);

    err = _RpcSend(gState.ioServerPort, static_cast<uint32_t>(FileIoEpType::CloseFile),
            requestBuf, gState.replyPort);
    if(err) {
        err = -2;
        goto fail;
    }

    // receive the response
    rxMsg = (struct MessageHeader *) rxBuf;
    err = PortReceive(gState.replyPort, rxMsg, kReplyBufSize, UINTPTR_MAX);

    mtx_unlock(&gState.lock);

    if(err > 0) {
        // read out the type
        if(rxMsg->receivedBytes < sizeof(RpcPacket)) {
            err = -3;
            goto fail;
        }

        const auto packet = reinterpret_cast<RpcPacket *>(rxMsg->data);
        if(packet->type != static_cast<uint32_t>(FileIoEpType::CloseFileReply)) {
            fprintf(stderr, "%s received wrong packet type %08x!\n", __PRETTY_FUNCTION__, packet->type);
            err = -3;
            goto fail;
        }

        // deserialize the capabilities response
        auto data = std::span(packet->payload, err - sizeof(RpcPacket));
        auto req = cista::deserialize<FileIoCloseReply>(data);

        err = req->status;
    } 
    // message too short for even the header
    else {
        err = -3;
        goto fail;
    }

    free(rxBuf);
    return err;

fail:;
    // failure case: ensures the lock is released
    mtx_unlock(&gState.lock);

    free(rxBuf);
    return err;
}
