#include "task_private.h"
#include "rpc/task.h"
#include "helpers/Send.h"

#include <cstdint>

#include <rpc/RpcPacket.hpp>
#include <rpc/RootSrvTaskEndpoint.hpp>

#include <cista/serialization.h>

using namespace task;
using namespace rpc;

/**
 * Task creation function
 *
 */
int RpcTaskCreate(const char *path, const char **args, uintptr_t *outHandle) {
    int err;
    struct MessageHeader *rxMsg = nullptr;

    // perform one-time init if needed
    call_once(&gStateOnceFlag, []{
        Init();
    });

    // build up message
    RootSrvTaskCreate req;

    req.path = std::string(path);

    if(args) {
        const char *arg = args[0];
        for(size_t i = 0; arg; i++, arg = args[i]) {
            req.args.push_back(std::string(arg));
        }
    }

    // acquire lock
    err = mtx_lock(&gState.lock);
    if(err != thrd_success) return -1;

    // send it
    auto buf = cista::serialize(req);
    err = _RpcSend(gState.serverPort, static_cast<uint32_t>(RootSrvTaskEpType::TaskCreate),
            buf, gState.replyPort);

    if(err) {
        err = -1;
        goto fail;
    }

    // read reply
    rxMsg = (struct MessageHeader *) gState.msgBuf;
    err = PortReceive(gState.replyPort, rxMsg, gState.msgBufLen, UINTPTR_MAX);

    mtx_unlock(&gState.lock);

    if(err > 0) {
        // read out the type
        if(rxMsg->receivedBytes < sizeof(RpcPacket)) {
            err = -2;
            goto fail;
        }

        const auto packet = reinterpret_cast<RpcPacket *>(rxMsg->data);
        if(packet->type != static_cast<uint32_t>(RootSrvTaskEpType::TaskCreateReply)) {
            fprintf(stderr, "%s received wrong packet type %08x!\n", __FUNCTION__, packet->type);
            err = -3;
            goto fail;
        }

        // deserialize the response
        auto data = std::span(packet->payload, err - sizeof(RpcPacket));
        auto res = cista::deserialize<RootSrvTaskCreateReply>(data);
        if(res->status) {
            err = res->status;
        } else {
            err = 0;

            if(outHandle) {
                *outHandle = res->handle;
            }
        }
    }
    // failed to receive
    else {
        goto fail;
    }

    // success
    return err;

fail:;
    // failure case: ensures the lock is released
    mtx_unlock(&gState.lock);
    return err;
}

