#include "task_private.h"
#include "rpc/task.h"
#include "helpers/Send.h"

#include <cstdint>
#include <cstdio>

#include <rpc/RpcPacket.hpp>
#include <rootsrv/TaskEndpoint.h>
#include <mpack/mpack.h>

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

    // set up the mpack writer
    char *reqData{nullptr};
    size_t reqSize{0};

    mpack_writer_t writer;
    mpack_writer_init_growable(&writer, &reqData, &reqSize);

    mpack_start_map(&writer, 3);

    // write the path
    mpack_write_cstr(&writer, "path");
    mpack_write_cstr(&writer, path);

    // arguments
    mpack_write_cstr(&writer, "args");
    if(args) {
        // count number of args
        size_t nArgs = 0;
        for(const char *arg = args[0]; arg; nArgs++, arg = args[nArgs]) { }

        // set up the list
        if(nArgs) {
            mpack_start_array(&writer, nArgs);

            const char *arg = args[0];
            for(size_t i = 0; arg; i++, arg = args[i]) {
                mpack_write_cstr(&writer, arg);
            }

            mpack_finish_array(&writer);
        }
        // no args: write nil
        else {
            mpack_write_nil(&writer);
        }
    }

    // finish up the param
    mpack_write_cstr(&writer, "flags");
    mpack_write_u32(&writer, 0);

    mpack_finish_map(&writer);

    auto status = mpack_writer_destroy(&writer);
    if(status != mpack_ok) {
        fprintf(stderr, "[rpc] %s failed: %d", "mpack_writer_destroy", status);
        return -1;
    }

    // acquire lock
    err = mtx_lock(&gState.lock);
    if(err != thrd_success) return -1;

    // send it
    std::span<uint8_t> request(reinterpret_cast<uint8_t *>(reqData), reqSize);
    err = rpc::RpcSend(gState.serverPort,
            static_cast<uint32_t>(TaskEndpointType::CreateTaskRequest), request, gState.replyPort);

    free(reqData);

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
        if(packet->type != static_cast<uint32_t>(TaskEndpointType::CreateTaskReply)) {
            fprintf(stderr, "[rpc] %s received wrong packet type %08x!\n", __FUNCTION__, packet->type);
            err = -3;
            goto fail;
        }

        // deserialize the response
        auto resBuf = std::span(packet->payload, rxMsg->receivedBytes - sizeof(RpcPacket));

        mpack_tree_t tree;
        mpack_tree_init_data(&tree, reinterpret_cast<const char *>(resBuf.data()), resBuf.size());
        mpack_tree_parse(&tree);
        mpack_node_t root = mpack_tree_root(&tree);

        const auto status = mpack_node_i32(mpack_node_map_cstr(root, "status"));

        if(status != 1) {
            err = status;
        } else {
            err = 0;

            if(outHandle) {
                // XXX: is there a cleaner way to get the handle size?
#if defined(__i386__)
                *outHandle = mpack_node_u32(mpack_node_map_cstr(root, "handle"));
#elif defined(__x86_64__)
                *outHandle = mpack_node_u64(mpack_node_map_cstr(root, "handle"));
#else
#error what the hell
#endif
            }
        }

        mpack_tree_destroy(&tree);
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

