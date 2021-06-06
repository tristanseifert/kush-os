/**
 * Defines some general types for RPC packets
 */
#ifndef RPC_SYSTEM_RPCPACKET_H
#define RPC_SYSTEM_RPCPACKET_H

namespace rpc {
/**
 * Wraps each RPC messages to provide a message type that can be further used to differentiate
 * the message contained within.
 *
 * The total size of the data is calculated by subtracting the size of the fixed header from the
 * total received message.
 */
struct RpcPacket {
    /// message type
    uint32_t type;
    /// port to send replies to, if any are requested
    uintptr_t replyPort;
    /// serialized message
    char payload[];
};
}

#endif
