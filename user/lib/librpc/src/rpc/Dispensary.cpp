#include "rpc/dispensary.h"
#include "sys/infopage.h"
#include "sys/syscalls.h"
#include "rpc_internal.h"
#include "helpers/Send.h"

#include <malloc.h>
#include <threads.h>

#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <span>
#include <vector>

#include <rpc/RpcPacket.hpp>
#include <rpc/RootSrvDispensaryEndpoint.hpp>

using namespace rpc;

/// Memory to allocate for the message receive struct
constexpr static const size_t kMaxMsgLen = 512 + sizeof(struct MessageHeader);

/// Lock to protect the port from concurrent use
LIBRPC_INTERNAL static mtx_t gLookupReplyPortLock;
/// Flag to ensure the receive port for lookups is only created once.
LIBRPC_INTERNAL static once_flag gLookupReplyPortCreatedFlag;
/// Port handle for service lookups
LIBRPC_INTERNAL static uintptr_t gLookupReplyPort = 0;

/// Receive buffer for replies from the port
LIBRPC_INTERNAL static void *gRxBuffer = nullptr;

/**
 * Performs one-time initialization of the lookup machinery.
 *
 * This entails allocating a port to receive replies on, and a lock to ensure only one thread will
 * go through this code at once. Lookups will entail a context switch anyways so the performance
 * impact of this isn't too great. Handles should be cached whenever possible.
 */
LIBRPC_INTERNAL static void InitDispensary() {
    int err;

    // create the port
    err = PortCreate(&gLookupReplyPort);
    assert(!err);

    // and the lock
    err = mtx_init(&gLookupReplyPortLock, mtx_plain);
    assert(err == thrd_success);

    // then, allocate the receive buffer
    err = posix_memalign(&gRxBuffer, 16, kMaxMsgLen);
    assert(!err);

    memset(gRxBuffer, 0, kMaxMsgLen);
}

/**
 * Attempts to resolve a name into a port.
 *
 * All RPC requests will block forever. This is in theory not a problem, assuming the root server
 * never goes away...
 *
 * @param name Service name to look up; this is a zero-terminated UTF-8 string.
 * @param outPort If a port is found, its handle is written to this variable.
 *
 * @return 0 if the request was completed, but the port was not found; 1 if the port was found; or
 * a negative error code.
 */
int LookupService(const char * _Nonnull _name, uintptr_t * _Nonnull outPort) {\
    int err;
    std::span<uint8_t> buf;

    // validate string inputs
    const auto nameLen = strlen(_name);
    const auto packetLen = sizeof(RootSrvDispensaryLookup) + nameLen + 1;

    if(nameLen > MAX_SERVICE_NAME || packetLen > kMaxMsgLen) {
        errno = EINVAL;
        return -1;
    }

    // fail if no dispensary port
    if(!__kush_infopg->dispensaryPort) {
        return -420;
    }

    // create the port for replies, if needed
    call_once(&gLookupReplyPortCreatedFlag, []{
        InitDispensary();
    });

    // acquire the lock
    err = mtx_lock(&gLookupReplyPortLock);
    if(err != thrd_success) {
        // cannot goto fail since the lock isn't locked
        return err;
    }

    // build the send request
    auto req = reinterpret_cast<RootSrvDispensaryLookup *>(gRxBuffer);
    memset(req, 0, packetLen);

    req->nameLen = nameLen;
    memcpy(req->name, _name, nameLen);

    buf = std::span<uint8_t>(reinterpret_cast<uint8_t *>(req), packetLen);

    err = _RpcSend(__kush_infopg->dispensaryPort,
            static_cast<uint32_t>(RootSrvDispensaryEpType::Lookup), buf, gLookupReplyPort);

    if(err) {
        goto fail;
    }

    // receive the reply
    err = PortReceive(gLookupReplyPort, reinterpret_cast<struct MessageHeader *>(gRxBuffer),
            kMaxMsgLen, UINTPTR_MAX);

    if(err < 0) {
        goto fail;
    }

    // interpret the reply
    {
        const auto rxMsg = reinterpret_cast<struct MessageHeader *>(gRxBuffer);

        // deserialize the request
        auto packet = reinterpret_cast<RpcPacket *>(rxMsg->data);

        auto data = std::span(packet->payload, std::min(err - sizeof(RpcPacket), (unsigned int) err));
        if(data.size() < sizeof(RootSrvDispensaryLookupReply)) {
            err = -1;
            goto fail;
        }

        auto reply = reinterpret_cast<const RootSrvDispensaryLookupReply *>(data.data());
        if(reply->nameLen > (data.size() - sizeof(RootSrvDispensaryLookupReply))) {
            // name was truncated
            err = -1;
            goto fail;
        }

        // make sure we got the right reply message
        if(strncmp(_name, reply->name, nameLen)) {
            err = -1;
            goto fail;
        }

        // extract handle
        if(!reply->status) {
            err = 1;
            *outPort = reply->port;
        } else {
            err = 0;
            *outPort = 0;
        }
    }

    // clean up
    mtx_unlock(&gLookupReplyPortLock);
    return err;

fail:;
    // failure handler; ensures we release memory and unlock again
    mtx_unlock(&gLookupReplyPortLock);
    return err;
}

/**
 * Registers a named service.
 *
 * @param name Service name to register under; this is a zero-terminated UTF-8 string.
 * @param port Port handle to register
 *
 * @return 0 if request was completed and port registered successfully; an error code otherwise.
 */
int RegisterService(const char * _Nonnull _name, const uintptr_t port) {
    int err;
    std::span<uint8_t> buf;

    // validate string inputs
    const auto nameLen = strlen(_name);
    const auto packetLen = sizeof(RootSrvDispensaryRegister) + nameLen + 1;

    if(nameLen > MAX_SERVICE_NAME || packetLen > kMaxMsgLen) {
        errno = EINVAL;
        return -1;
    }

    // fail if no dispensary port
    if(!__kush_infopg->dispensaryPort) {
        return -420;
    }
    // create the port for replies, if needed
    call_once(&gLookupReplyPortCreatedFlag, []{
        InitDispensary();
    });

    // acquire lock
    err = mtx_lock(&gLookupReplyPortLock);
    if(err != thrd_success) {
        // cannot goto fail since the lock isn't locked
        return err;
    }

    // build the send request
    auto req = reinterpret_cast<RootSrvDispensaryRegister *>(gRxBuffer);
    memset(req, 0, packetLen);

    req->portHandle = port;
    req->nameLen = nameLen;
    memcpy(req->name, _name, nameLen);

    buf = std::span<uint8_t>(reinterpret_cast<uint8_t *>(req), packetLen);

    // and send it
    err = _RpcSend(__kush_infopg->dispensaryPort,
            static_cast<uint32_t>(RootSrvDispensaryEpType::Register), buf, gLookupReplyPort);
    if(err) {
        goto fail;
    }

    // receive the reply
    err = PortReceive(gLookupReplyPort, reinterpret_cast<struct MessageHeader *>(gRxBuffer),
            kMaxMsgLen, UINTPTR_MAX);

    if(err < 0) {
        goto fail;
    }

    // interpret the reply
    {
        const auto rxMsg = reinterpret_cast<struct MessageHeader *>(gRxBuffer);

        // deserialize the request
        auto packet = reinterpret_cast<RpcPacket *>(rxMsg->data);
        auto data = std::span(packet->payload, std::min(err - sizeof(RpcPacket), (unsigned int) err));
        if(data.size() < sizeof(RootSrvDispensaryRegisterReply)) {
            err = -1;
            goto fail;
        }

        auto reply = reinterpret_cast<const RootSrvDispensaryRegisterReply *>(data.data());
        if(reply->nameLen > (data.size() - sizeof(RootSrvDispensaryRegisterReply))) {
            err = -1;
            goto fail;
        }

        // make sure we got the right reply message
        if(strncmp(_name, reply->name, nameLen)) {
            err = -1;
            goto fail;
        }

        // extract handle
        // XXX: do we care about the "did overwrite" flag?
        err = reply->status;
    }

    // clean up
    mtx_unlock(&gLookupReplyPortLock);

    return err;

fail:;
    // failure handler; ensures we release memory and unlock again
    mtx_unlock(&gLookupReplyPortLock);
    return err;
    return -1;
}
