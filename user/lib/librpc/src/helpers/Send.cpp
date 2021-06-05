#include "Send.h"
#include "rpc/Helpers.hpp"

#include <sys/syscalls.h>
#include <rpc/RpcPacket.hpp>
#include <malloc.h>

#include <span>
#include <system_error>

#include <capnp/message.h>
#include <capnp/serialize.h>

using namespace rpc;

/**
 * Sends an RPC message.
 *
 * @param replyTo Value to insert into the "reply port" value in the RPC header.
 */
int rpc::RpcSend(const uintptr_t port, const uint32_t type, const std::span<uint8_t> &buf,
        const uintptr_t replyTo) {
    int err;
    void *txBuf = nullptr;

    // allocate the reply buffer
    const auto replySize = buf.size() + sizeof(rpc::RpcPacket);
    err = posix_memalign(&txBuf, 16, replySize);
    if(err) {
        return err;
    }

    auto txPacket = reinterpret_cast<rpc::RpcPacket *>(txBuf);
    txPacket->type = type;
    txPacket->replyPort = replyTo;

    memcpy(txPacket->payload, buf.data(), buf.size());

    // send it
    err = PortSend(port, txPacket, replySize);
    free(txBuf);

    if(err) {
        return err;
    }

    return 0;
}

/**
 * Sends an RPC message.
 *
 * @param message Cap'n Proto message builder whose segments are to be serialized
 * @param replyTo Value to insert into the "reply port" value in the RPC header.
 */
int rpc::RpcSend(const uintptr_t port, const uint32_t type, capnp::MessageBuilder &message,
        const uintptr_t replyTo) {
    int err;
    void *txBuf = nullptr;

    // get message to bytes and calculate its final size
    auto msgWords = capnp::messageToFlatArray(message);
    auto msgBytes = msgWords.asBytes();

    // allocate the reply buffer
    const auto replySize = msgBytes.size() + sizeof(rpc::RpcPacket);
    err = posix_memalign(&txBuf, 16, replySize);
    if(err) {
        return err;
    }

    auto txPacket = reinterpret_cast<rpc::RpcPacket *>(txBuf);
    txPacket->type = type;
    txPacket->replyPort = replyTo;

    // write it as a whole me
    memcpy(txPacket->payload, msgBytes.begin(), msgBytes.size());

    // send it
    err = PortSend(port, txPacket, replySize);
    free(txBuf);

    if(err) {
        return err;
    }

    return 0;
}

int _RpcSendReply(const rpc::RpcPacket *packet, const uint32_t type, const std::span<uint8_t> &buf) {
    return RpcSend(packet->replyPort, type, buf);
}

int _RpcSendReply(const rpc::RpcPacket *packet, const uint32_t type, capnp::MessageBuilder &msg) {
    return RpcSend(packet->replyPort, type, msg);
}

