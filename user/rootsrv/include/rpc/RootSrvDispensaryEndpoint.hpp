/*
 * Defines RPC structures for the dispensary endpoint, which can be used to look up names and
 * convert them to port handles.
 */
#ifndef RPC_ROOTSRV_DISPENSARYEP_HPP
#define RPC_ROOTSRV_DISPENSARYEP_HPP

#include <cstdint>

#include <cista/containers/string.h>

namespace rpc {

/**
 * Message type
 */
enum class RootSrvDispensaryEpType: uint32_t {
    /// Flag indicating reply
    ReplyFlag                           = 0x80000000,

    /// Client -> server; look up port
    Lookup                              = 'LOOK',
    /// Server -> client; look up response
    LookupReply                         = Lookup | ReplyFlag,

    /// Registers a port
    Register                            = 'REGP',
    /// Registration reply
    RegisterReply                       = Register | ReplyFlag,
};

/**
 * Request the port corresponding to the given string name.
 */
struct RootSrvDispensaryLookup {
    /// name to resolve
    cista::offset::string name;
};
/**
 * Response to a previous request to look up a port.
 */
struct RootSrvDispensaryLookupReply {
    /// name that was requested to be resolved
    cista::offset::string name;

    /// status code: 0 indicates success
    intptr_t status;
    /// port handle, if the lookup was successful
    uintptr_t port;
};


/**
 * Registers a port under the specified name.
 */
struct RootSrvDispensaryRegister {
    /// name under which the port is to be registered
    cista::offset::string name;
    /// port handle
    uintptr_t portHandle;
};
/**
 * Response to a registration request.
 */
struct RootSrvDispensaryRegisterReply {
    /// port name that was registered
    cista::offset::string name;

    /// registration status: 0 = success, any other = error
    int32_t status;
    /// when set, an existing registration under this name was replaced
    bool replaced;
};

}

#endif
