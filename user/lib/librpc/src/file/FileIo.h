#ifndef _LIBRPC_RPC_FILEIO_H
#define _LIBRPC_RPC_FILEIO_H

#include "rpc_internal.h"

#include <sys/bitflags.hpp>
#include <cstdint>

#include <threads.h>

/**
 * Different actions and IO methods that may be used to communicate with the currently selected
 * file IO server
 */
ENUM_FLAGS(ServerCaps);
enum class ServerCaps: uintptr_t {
    /// default capabilities (no features)
    Default                             = 0,

    /// direct IO is possible
    DirectIo                            = (1 << 0),
};

/**
 * Info structure for the state of the file IO system
 */
struct FileIoState {
    // lock to protect access to the resources
    mtx_t lock;
    // port to send requests to
    uintptr_t ioServerPort;
    // port for receiving file IO replies
    uintptr_t replyPort;

    /// various supported capabilities
    ServerCaps caps;
    /// maximum IO block size
    uintptr_t maxIoSize;
};

namespace fileio {
extern once_flag gStateOnceFlag;
extern FileIoState gState;

LIBRPC_INTERNAL void Init();

LIBRPC_INTERNAL bool UpdateServerPort();
}

#endif
