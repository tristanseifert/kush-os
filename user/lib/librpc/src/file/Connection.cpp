#include "FileIo.h"

#include "rpc/dispensary.h"
#include "rpc/file.h"
#include "sys/infopage.h"
#include "sys/syscalls.h"

#include "helpers/Send.h"
#include "rpc_internal.h"

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

namespace fileio {
static bool Connect();
static bool UpdateCaps();
}

/// ensures global state is initialized only once
LIBRPC_INTERNAL once_flag fileio::gStateOnceFlag;
/// global file IO state
LIBRPC_INTERNAL FileIoState fileio::gState;

/**
 * Initializes file IO.
 */
void fileio::Init() {
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
static bool fileio::Connect() {
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
static bool fileio::UpdateCaps() {
    int err;
    void *rxBuf = nullptr;

    // serialize the request
    FileIoGetCaps req;
    req.requestedVersion = 1;

    auto requestBuf = std::span<uint8_t>(reinterpret_cast<uint8_t *>(&req), sizeof(FileIoGetCaps));
    err = _RpcSend(gState.ioServerPort, static_cast<uint32_t>(FileIoEpType::GetCapabilities),
            requestBuf, gState.replyPort);
    if(err) return false;

    // allocate a receive buffer
    constexpr static const size_t kReplyBufSize = 256 + sizeof(struct MessageHeader);
    err = posix_memalign(&rxBuf, 16, kReplyBufSize);
    if(err) {
        return err;
    }

    memset(rxBuf, 0, kReplyBufSize);

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
            fprintf(stderr, "%s received wrong packet type %08x!\n", __FUNCTION__, packet->type);
            goto fail;
        }

        // deserialize the capabilities response
        auto data = std::span(packet->payload, err - sizeof(RpcPacket));
        if(data.size() < sizeof(FileIoGetCapsReply)) {
            goto fail;
        }
        auto req = reinterpret_cast<const FileIoGetCapsReply *>(data.data());

        // read out capabilities
        gState.caps = ServerCaps::Default;

        if(TestFlags(req->capabilities & FileIoCaps::DirectIo)) {
            gState.caps |= ServerCaps::DirectIo;
        }

        gState.maxIoSize = req->maxReadBlockSize;

        //fprintf(stderr, "supported capabilities: %08x block max %u\n", (uintptr_t) gState.caps,
        //        gState.maxIoSize);
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
 * Resolves the port handle for the file IO service and updates the capabilities field.
 */
bool fileio::UpdateServerPort() {
    // determine port
    if(!Connect()) return false;
    // determine capabilities
    if(!UpdateCaps()) return false;

    return true;
}

