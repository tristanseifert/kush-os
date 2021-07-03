#include "MessageLoop.h"
#include "Log.h"
#include "PacketTypes.h"

#include <rpc/dispensary.h>
#include <rpc/RpcPacket.hpp>
#include <sys/elf.h>
#include <sys/syscalls.h>

#include <cstdlib>

/**
 * Initializes the message loop, including its receive buffer. This will also create the port and
 * register it.
 */
MessageLoop::MessageLoop() {
    int err;

    // allocate receive buffer
    err = posix_memalign(&this->rxBuf, 16, kMaxMsgLen);
    if(err) {
        Abort("%s failed: %d", "posix_memalign", err);
    }

    // allocate port and register it
    err = PortCreate(&this->port);
    if(err) {
        Abort("%s failed: %d", "PortCreate", err);
    }

    err = RegisterService(kPortName.data(), this->port);
    if(err) {
        Abort("%s failed: %d", "RegisterService", err);
    }
}

/**
 * Tears down message loop resources.
 */
MessageLoop::~MessageLoop() {
    PortDestroy(this->port);
    free(this->rxBuf);
}

/**
 * Main loop; we'll wait to receive messages forever from the port.
 */
void MessageLoop::run() {
    int err;

    // set up receive buffer
    auto msg = reinterpret_cast<struct MessageHeader *>(this->rxBuf);
    memset(msg, 0, sizeof(*msg));

    // wait to receive messages
    Success("Waiting to receive messages on port $%p'h", this->port);

    while(true) {
        err = PortReceive(this->port, msg, kMaxMsgLen, UINTPTR_MAX);

        if(err < 0) {
            Abort("%s failed: %d", "PortReceive", err);
        } else if(msg->receivedBytes < sizeof(rpc::RpcPacket)) {
            Warn("Received too small RPC message (%lu bytes)", msg->receivedBytes);
            continue;
        }

        // handle the packet
        auto packet = reinterpret_cast<rpc::RpcPacket *>(msg->data);

        switch(packet->type) {
            case static_cast<uint32_t>(DyldosrvMessageType::MapSegment):
                this->handleMapSegment(msg);
                break;

            default:
                Warn("Unknown RPC message type: $%08x", packet->type);
                break;
        }
    }
}

/**
 * Handles a request to map a segment of a dynamic library.
 */
void MessageLoop::handleMapSegment(struct MessageHeader *msg) {
    // validate it
    auto packet = reinterpret_cast<rpc::RpcPacket *>(msg->data);
    const size_t payloadBytes = msg->receivedBytes - sizeof(rpc::RpcPacket);
    if(payloadBytes < sizeof(DyldosrvMapSegmentRequest)) {
        Warn("Received too small RPC message (%lu bytes, type $%08x)", msg->receivedBytes,
                packet->type);
        return;
    }

    auto req = reinterpret_cast<DyldosrvMapSegmentRequest *>(packet->payload);
    const size_t nameBytes = payloadBytes - sizeof(DyldosrvMapSegmentRequest);
    if(nameBytes < 2) {
        Warn("Name length too short!");
        return;
    }

    // TODO: implement
    Warn("Try load '%s' off $%07x at vm $%p in $%p'h", req->path, req->phdr.p_offset,
            req->phdr.p_vaddr + req->objectVmBase, msg->senderTask);
}
