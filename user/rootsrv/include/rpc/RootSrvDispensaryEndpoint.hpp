/*
 * Defines RPC structures for the dispensary endpoint, which can be used to look up names and
 * convert them to port handles.
 */
#ifndef RPC_ROOTSRV_DISPENSARYEP_HPP
#define RPC_ROOTSRV_DISPENSARYEP_HPP

#include <cstdint>

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
    /// length of name
    uint16_t nameLen;
    /// name to resolve
    char name[];
};
/**
 * Response to a previous request to look up a port.
 */
struct RootSrvDispensaryLookupReply {
    /// status code: 0 indicates success
    intptr_t status;
    /// port handle, if the lookup was successful
    uintptr_t port;

    /// length of name that was resolved
    uint16_t nameLen;
    /// name that this request resolved
    char name[];
};


/**
 * Registers a port under the specified name.
 */
struct RootSrvDispensaryRegister {
    /// port handle
    uintptr_t portHandle;

    /// length of name
    uint16_t nameLen;
    /// name to register the port under
    char name[];
};
/**
 * Response to a registration request.
 */
struct RootSrvDispensaryRegisterReply {
    /// registration status: 0 = success, any other = error
    int32_t status;
    /// when set, an existing registration under this name was replaced
    bool replaced;

    /// length of name
    uint16_t nameLen;
    // port name that was registered
    char name[];
};

}

#endif
