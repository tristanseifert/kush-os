/**
 * Provides the interface on the dyldo task loader port.
 */
#ifndef RPC_DYLDO_LOADERPORT
#define RPC_DYLDO_LOADERPORT

#include <cstdint>

#include <cista/containers/string.h>

#include <sys/bitflags.hpp>

namespace rpc {
/**
 * Message type
 */
enum class DyldoLoaderEpType: uint32_t {
    // flag to indicate a reply
    ReplyFlag                           = 0x80000000,

    /// boostrap a newly created task
    TaskCreated                         = 'BOOT',
    /// reply to a boostrap request
    TaskCreatedReply                    = TaskCreated | ReplyFlag,
};

/**
 * Request to indicate a new task has been created
 */
struct DyldoLoaderTaskCreated {
    /// Task handle
    uintptr_t taskHandle;
    /// Path from which the task was loaded
    cista::offset::string binaryPath;
};
/**
 * Reply to the task creation.
 */
struct DyldoLoaderTaskCreatedReply {
    /// status code: 0 = success
    int32_t status;
    /// Task handle that we just processed
    uintptr_t taskHandle;
    /// New entry point
    uintptr_t entryPoint;
};

}

#endif
