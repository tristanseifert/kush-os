#include "Send.h"

#include <sys/syscalls.h>
#include <rpc/RpcPacket.hpp>
#include <malloc.h>

#include <span>
#include <system_error>

/**
 * Sends an RPC message.
 *
 * @param replyTo Value to insert into the "reply port" value in the RPC header.
 */
int _RpcSend(const uintptr_t port, const uint32_t type, const std::span<uint8_t> &buf,
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

int _RpcSendReply(const rpc::RpcPacket *packet, const uint32_t type, const std::span<uint8_t> &buf) {
    return _RpcSend(packet->replyPort, type, buf);
}

