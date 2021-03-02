#include "driver/Messages.hpp"

#include <malloc.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <system_error>

#include <rpc/dispensary.h>
#include <rpc/RpcPacket.hpp>
#include <mpack/mpack.h>

using namespace libdriver;

/// Init flag
static std::once_flag gInitFlag;
/// Global state
static struct {
    /// lock on the buffer
    std::mutex bufferLock;
    /// Send/receive buffer
    void *buffer;
    /// length of the buffer
    size_t bufferLen;

    /// Server port
    uintptr_t serverPort;
} gState;

/**
 * Initializes the state structure.
 */
static void Init() {
    int err;

    // allocate the buffer
    constexpr static const size_t kBufferSz = (1024 * 4);
    gState.bufferLen = kBufferSz;

    err = posix_memalign(&gState.buffer, 16, gState.bufferLen);
    if(err) {
        throw std::system_error(err, std::generic_category(), "posix_memalign");
    }
    memset(gState.buffer, 0, gState.bufferLen);

    // resolve server handle
    err = LookupService("me.blraaz.rpc.driverman", &gState.serverPort);
    if(err != 1) {
        throw std::runtime_error("failed to resolve driverman handle");
    }
}



/**
 * Sends a message to the driver server, optionally including a reply port.
 */
void libdriver::SendToSrv(Message *msg, const uintptr_t replyPort) {
    int err;

    // initialize buffers and resolve port
    std::call_once(gInitFlag, []{
        Init();
    });

    std::lock_guard<std::mutex> lg(gState.bufferLock);

    // fill packet header
    auto packet = reinterpret_cast<rpc::RpcPacket *>(gState.buffer);
    packet->type = msg->getRpcType();
    packet->replyPort = replyPort;

    assert(packet->type);

    // serialize the message
    const auto msgMaxLen = gState.bufferLen - sizeof(rpc::RpcPacket);
    auto msgSpan = std::span<std::byte>(reinterpret_cast<std::byte *>(packet->payload), msgMaxLen);
    const auto msgBytes = msg->serialize(msgSpan);

    // send it
    const auto totalBytes = msgBytes + sizeof(rpc::RpcPacket);
    err = PortSend(gState.serverPort, packet, totalBytes);
    if(err < 0) {
        throw std::system_error(err, std::generic_category(), "PortSend");
    }
}
