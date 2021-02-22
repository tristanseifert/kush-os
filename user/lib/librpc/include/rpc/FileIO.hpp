/*
 * RPC interface for file-based IO.
 *
 * This interface is implemented by various different servers; before making the first request to
 * perform any IO, you should do a GetCapabilities request to see what calls you can make for
 * actual file IO.
 */
#ifndef RPC_SYSTEM_FILEIO_HPP
#define RPC_SYSTEM_FILEIO_HPP

#include <cstdint>

#include <cista/containers/string.h>
#include <cista/containers/vector.h>

namespace rpc {
/**
 * Message type
 */
enum class FileIoEpType: uint32_t {

};

}

#endif
