#ifndef TASK_PRIVATE_H
#define TASK_PRIVATE_H

#include "rpc_internal.h"

#include <threads.h>

namespace task {
struct IoState {
    // lock to protect access to the resources
    mtx_t lock;
    // port to send requests to
    uintptr_t serverPort = 0;
    // port for receiving file IO replies
    uintptr_t replyPort = 0;

    /// message buffer (rx + tx)
    void *msgBuf = nullptr;
    /// length of message buffer, bytes
    size_t msgBufLen = 0;
};

/// Global state to communicate with task server
extern IoState gState;
/// once flag for initialization
extern once_flag gStateOnceFlag;


/// Task RPC initialization
LIBRPC_INTERNAL void Init();
/// Resolves the port to the task endpoint.
LIBRPC_INTERNAL void UpdateServerPort();
}

#endif
