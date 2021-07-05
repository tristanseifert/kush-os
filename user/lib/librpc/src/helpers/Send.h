#ifndef _LIBRPC_HELPERS_SEND_H
#define _LIBRPC_HELPERS_SEND_H

#include "rpc_internal.h"
#include "rpc/Helpers.hpp"

#include <cstdint>
#include <span>

namespace rpc {
struct RpcPacket;
}

/**
 * Replies to a received RPC message.
 */
LIBRPC_INTERNAL int _RpcSendReply(const rpc::RpcPacket *packet, const uint32_t type, const std::span<uint8_t> &buf);

#endif
