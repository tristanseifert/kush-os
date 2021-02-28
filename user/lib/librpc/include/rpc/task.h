#ifndef LIBRPC_RPC_TASK_H
#define LIBRPC_RPC_TASK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Creates a new task from the specified binary file.
 *
 * @param path Path to an executable to launch
 * @param args List of arguments to pass to the task; terminated with NULL.
 * @param outHandle Variable in which a handle to the created task is stored.
 *
 * @return A negative error code, or 0 on success.
 */
int RpcTaskCreate(const char * _Nonnull path, const char * _Nullable * _Nullable args,
        uintptr_t * _Nullable outHandle);

#ifdef __cplusplus
}
#endif

#endif
