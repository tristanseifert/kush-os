/*
 * Defines RPC structures for the tasks RPC endpoint in the root server.
 *
 * This endpoint is used to create processes, for example.
 */
#ifndef RPC_ROOTSRV_TASKEP_HPP
#define RPC_ROOTSRV_TASKEP_HPP

#include <cstdint>

#include <cista/containers/string.h>
#include <cista/containers/vector.h>

namespace rpc {

/**
 * Message type
 */
enum class RootSrvTaskEpType: uint32_t {
    /// Client -> server; create task
    kTaskCreate                         = 'TSKC',
    /// server -> client; create task response
    kTaskCreateReply                    = 'TSKR',
};

/**
 * Request to create a new task.
 *
 * The binary is expected to be a dynamically linked ELF. You may specify arguments that are
 * passed to the task as well.
 */
struct RootSrvTaskCreate {
    /// Path to the binary
    cista::offset::string path;
    /// optional arguments
    cista::offset::vector<cista::offset::string> args;

    /// if set, the task is started suspended
    bool suspended = false;
};

/**
 * Reply to the TaskCreate message.
 */
struct RootSrvTaskCreateReply {
    /// status code: 0 = success
    intptr_t status;

    /// Task handle
    uintptr_t handle;
};

}

#endif
