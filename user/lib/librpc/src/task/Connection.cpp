#include "task_private.h"

#include "rpc/task.h"
#include "rpc/dispensary.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <threads.h>

#include <sys/syscalls.h>

using namespace task;

/// Name of the service to look up
static const char *kServiceName = "me.blraaz.rpc.rootsrv.task";
/// Maximum message buffer size
constexpr static const size_t kMsgBufLen = (1024 * 4);

/// Global state for all task RPC calls
LIBRPC_INTERNAL IoState task::gState;
LIBRPC_INTERNAL once_flag task::gStateOnceFlag;

/**
 * Initializes the task RPC calls.
 */
void task::Init() {
    int err;

    // set up the structure lock
    memset(&gState, 0, sizeof(gState));

    err = mtx_init(&gState.lock, mtx_plain);
    assert(err == thrd_success);

    // allocate message buffer
    err = posix_memalign(&gState.msgBuf, 16, kMsgBufLen);
    assert(!err);

    gState.msgBufLen = kMsgBufLen;
    memset(gState.msgBuf, 0, kMsgBufLen);

    // allocate receive port
    err = PortCreate(&gState.replyPort);
    assert(!err);

    // resolve service
    UpdateServerPort();
}

/**
 * Resolve the RPC service name.
 */
void task::UpdateServerPort() {
    int err;

    err = LookupService(kServiceName, &gState.serverPort);
    if(err != 1) {
        fprintf(stderr, "[rpc] service lookup %s failed: %d\n", kServiceName, err);
        abort();
    }
}
