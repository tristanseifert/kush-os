#ifndef ROOTSRV_RPC_TASKENDPOINT_H
#define ROOTSRV_RPC_TASKENDPOINT_H

#include <cstdint>

/**
 * Message types
 */
enum class TaskEndpointType: uint32_t {
    CreateTaskRequest                   = 0x124C9DE8,
    CreateTaskReply                     = 0x924C9DE8,
};

#endif
