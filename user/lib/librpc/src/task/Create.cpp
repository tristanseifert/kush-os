#include "task_private.h"
#include "rpc/task.h"
#include "helpers/Send.h"

#include <cstdint>
#include <cstdio>

#include <rpc/RpcPacket.hpp>
#include "rpc/rootsrv/TaskEndpoint.capnp.h"

#include <capnp/message.h>
#include <capnp/serialize.h>

using namespace task;
using namespace rpc;
using namespace rpc::TaskEndpoint;

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
    capnp::MallocMessageBuilder requestBuilder;
    auto request = requestBuilder.initRoot<rpc::TaskEndpoint::CreateRequest>();

    request.setPath(path);

    if(args) {
        // count number of args
        size_t nArgs = 0;
        for(const char *arg = args[0]; arg; nArgs++, arg = args[nArgs]) { }

        // set up the list
        if(nArgs) {
            auto list = request.initArgs(nArgs);

            const char *arg = args[0];
            for(size_t i = 0; arg; i++, arg = args[i]) {
                list.set(i, arg);
            }
        }
    }

    // acquire lock
    err = mtx_lock(&gState.lock);
    if(err != thrd_success) return -1;

    // send it
    err = rpc::RpcSend(gState.serverPort, K_TYPE_CREATE_REQUEST, requestBuilder, gState.replyPort);

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
        if(packet->type != K_TYPE_CREATE_REPLY) {
            fprintf(stderr, "%s received wrong packet type %08x!\n", __FUNCTION__, packet->type);
            err = -3;
            goto fail;
        }

        // deserialize the response
        auto resBuf = std::span(packet->payload, err - sizeof(RpcPacket));
        kj::ArrayPtr<const capnp::word> message(reinterpret_cast<const capnp::word *>(resBuf.data()),
                resBuf.size() / sizeof(capnp::word));
        capnp::FlatArrayMessageReader responseReader(message);
        auto res = responseReader.getRoot<rpc::TaskEndpoint::CreateReply>();

        if(res.getStatus()) {
            err = res.getStatus();
        } else {
            err = 0;

            if(outHandle) {
                *outHandle = res.getHandle();
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

