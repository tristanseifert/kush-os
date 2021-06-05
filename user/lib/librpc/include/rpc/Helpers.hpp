#ifndef LIBRPC_RPC_HELPERS_H
#define LIBRPC_RPC_HELPERS_H

#include <cstddef>
#include <cstdint>
#include <span>

namespace capnp {
class MessageBuilder;
}

namespace rpc {
struct RpcPacket;

/**
 * Sends an RPC message.
 */
int RpcSend(const uintptr_t port, const uint32_t type, const std::span<uint8_t> &buf,
        const uintptr_t replyTo = 0);
int RpcSend(const uintptr_t port, const uint32_t type, capnp::MessageBuilder &msg,
        const uintptr_t replyTo = 0);
}

#endif
