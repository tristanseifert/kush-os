#ifndef _LIBRPC_HELPERS_SEND_H
#define _LIBRPC_HELPERS_SEND_H

#include <cstdint>
#include <span>

namespace rpc {
struct RpcPacket;
}

/**
 * Sends an RPC message.
 */
int _RpcSend(const uintptr_t port, const uint32_t type, const std::span<uint8_t> &buf,
        const uintptr_t replyTo = 0);

/**
 * Replies to a received RPC message.
 */
inline int _RpcSendReply(const rpc::RpcPacket *packet, const uint32_t type, const std::span<uint8_t> &buf);

#endif
