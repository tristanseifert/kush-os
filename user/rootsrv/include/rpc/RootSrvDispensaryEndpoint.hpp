/*
 * Defines RPC structures for the dispensary endpoint, which can be used to look up names and
 * convert them to port handles.
 */
#ifndef RPC_ROOTSRV_DISPENSARYEP_HPP
#define RPC_ROOTSRV_DISPENSARYEP_HPP

#include <cstdint>

#include <cista/containers/string.h>
#include <cista/containers/vector.h>

namespace rpc {

/**
 * Message type
 */
enum class RootSrvDispensaryEpType: uint32_t {
    /// Client -> server; look up port
    Lookup                              = 'LOOQ',
    /// Server -> client; look up response
    LookupReply                         = 'LOOR',
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

}

#endif
