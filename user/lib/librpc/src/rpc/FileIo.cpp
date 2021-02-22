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

/// ensures global state is initialized only once
static once_flag gStateOnceFlag;
/// global file IO state
static FileIoState gState;

/**
 * Initializes file IO.
 */
static void FileIoInit() {
    int err;

    // set up the structure lock
    memset(&gState, 0, sizeof(gState));

    err = mtx_init(&gState.lock, mtx_plain);
    assert(err == thrd_success);

    // allocate receive port
    err = PortCreate(&gState.replyPort);
    assert(!err);
}

/**
 * Finds the port to which file IO requests should be sent.
 */
static bool FileIoConnect() {
    int err;
    uintptr_t handle = 0;

    // try the file service
    err = LookupService("me.blraaz.rpc.fileio", &handle);
    if(err == 1) {
        gState.ioServerPort = handle;
        return true;
    }

    // next, try the init file service
    err = LookupService("me.blraaz.rpc.rootsrv.initfileio", &handle);
    if(err == 1) {
        gState.ioServerPort = handle;
        return true;
    }

    // if we get here, failed to connect
    return false;
}

/**
 * Sends a capabilities request to the IO handler.
 */
static bool FileIoUpdateCaps() {
    int err;
    void *rxBuf = nullptr;

    // serialize the request
    FileIoGetCaps req;
    req.requestedVersion = 1;

    auto requestBuf = cista::serialize(req);

    err = _RpcSend(gState.ioServerPort, static_cast<uint32_t>(FileIoEpType::GetCapabilities),
            requestBuf, gState.replyPort);
    if(err) return false;

    // allocate a receive buffer
    constexpr static const size_t kReplyBufSize = 256 + sizeof(struct MessageHeader);
    err = posix_memalign(&rxBuf, 16, kReplyBufSize);
    if(err) {
        return err;
    }

    // receive pls
    struct MessageHeader *msg = (struct MessageHeader *) rxBuf;
    err = PortReceive(gState.replyPort, msg, kReplyBufSize, UINTPTR_MAX);

    if(err > 0) {
        // read out the type
        if(msg->receivedBytes < sizeof(RpcPacket)) {
            goto fail;
        }

        const auto packet = reinterpret_cast<RpcPacket *>(msg->data);
        if(packet->type != static_cast<uint32_t>(FileIoEpType::GetCapabilitiesReply)) {
            fprintf(stderr, "%s received wrong packet type %08x!\n", __PRETTY_FUNCTION__, packet->type);
            goto fail;
        }

        // deserialize the capabilities response
        auto data = std::span(packet->payload, err - sizeof(RpcPacket));
        auto req = cista::deserialize<FileIoGetCapsReply>(data);

        // read out capabilities
        gState.caps = ServerCaps::Default;

        if(TestFlags(req->capabilities & FileIoCaps::DirectIo)) {
            gState.caps |= ServerCaps::DirectIo;
        }

        fprintf(stderr, "supported capabilities: %08x\n", (uintptr_t) gState.caps);
    } else {
        goto fail;
    }

    free(rxBuf);
    return true;

fail:;
    // error case
    free(rxBuf);
    return false;
}


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
        FileIoInit();
    });

    // acquire lock
    err = mtx_lock(&gState.lock);
    if(err != thrd_success) return -1;

    // perform the IO service lookup
    if(!gState.ioServerPort) {
        // determine port
        if(!FileIoConnect()) {
            err = -2;
            goto fail;
        }

        // determine capabilities
        if(!FileIoUpdateCaps()) {
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
        return err;
    }

    // receive pls
    rxMsg = (struct MessageHeader *) rxBuf;
    err = PortReceive(gState.replyPort, rxMsg, kReplyBufSize, UINTPTR_MAX);

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
    mtx_unlock(&gState.lock);
    return err;
}
