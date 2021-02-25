#include "DyldoPipe.h"
#include "Task.h"
#include "dispensary/Registry.h"

#include <malloc.h>
#include <sys/_infopage.h>
#include <sys/syscalls.h>
#include <rpc/RpcPacket.hpp>
#include "rpc/LoaderPort.h"

#include <cista/serialization.h>

#include <system_error>

#include "log.h"

using namespace task;

/// Name of the dynamic linker bootstrap port. This must match what the linker registers as
const std::string DyldoPipe::kDyldoPortName = "me.blraaz.rpc.rt.dyld.loader";

/**
 * Allocates a new dyldo pipe. This will allocate a reply port, as well as the buffer for message
 * IO. Then, we'll look up the name of the remote port, waiting for it to become available.
 */
DyldoPipe::DyldoPipe() {
    int err;

    // set up port and alloc message buffer
    err = PortCreate(&this->replyPort);
    if(err) {
        throw std::system_error(err, std::generic_category(), "PortCreate");
    }

    err = posix_memalign(&this->msgBuf, 16, kMsgBufLen);
    if(err) {
        throw std::system_error(err, std::generic_category(), "posix_memalign");
    }

    // wait and look up the port
    bool found = dispensary::Registry::lookup(kDyldoPortName, this->dyldoPort);
    if(!found) {
        throw std::runtime_error("failed to look up dyldo port");
    }
}

/**
 * Cleans up all allocated resources, including the message buffer and ports.
 */
DyldoPipe::~DyldoPipe() {
    std::lock_guard<std::mutex> lg(this->lock);

    if(this->replyPort) {
        PortDestroy(this->replyPort);
    }
    if(this->msgBuf) {
        free(this->msgBuf);
    }
}

/**
 * Notifies the dynamic linker that the given task has launched.
 *
 * This will map the prelinked libraries (which includes the dynamic linker runtime) into the
 * task's address space.
 *
 * @return 0 if successful, error code otherwise.
 */
int DyldoPipe::taskLaunched(Task *t, uintptr_t &linkerEntry) {
    using namespace rpc;

    int err;
    std::lock_guard<std::mutex> lg(this->lock);

    // create the message
    DyldoLoaderTaskCreated req;
    req.taskHandle = t->getHandle();
    req.binaryPath = t->getPath();

    auto buf = cista::serialize(req);

    // build the RPC packet and send
    auto packet = reinterpret_cast<RpcPacket *>(this->msgBuf);
    packet->type = static_cast<uint32_t>(DyldoLoaderEpType::TaskCreated);
    packet->replyPort = this->replyPort;

    memcpy(packet->payload, buf.data(), buf.size()); // XXX: bounds check

    err = PortSend(this->dyldoPort, packet, sizeof(RpcPacket) + buf.size());

    if(err) {
        return err;
    }

    // get the reply port
    struct MessageHeader *msg = (struct MessageHeader *) this->msgBuf;
    err = PortReceive(this->replyPort, msg, kMsgBufLen, UINTPTR_MAX);

    if(err > 0) {
        if(msg->receivedBytes < sizeof(RpcPacket)) {
            return -1;
        }

        // deserialize the request
        const auto packet = reinterpret_cast<RpcPacket *>(msg->data);
        if(packet->type != static_cast<uint32_t>(DyldoLoaderEpType::TaskCreatedReply)) {
            LOG("Invalid reply type: %08x", packet->type);
            return -2;
        }

        auto data = std::span(packet->payload, msg->receivedBytes - sizeof(RpcPacket));
        auto reply = cista::deserialize<DyldoLoaderTaskCreatedReply>(data);

        if(reply->taskHandle != t->getHandle()) {
            LOG("Received dyldo reply for task $%08x'h but expected $%08x'h",
                    reply->taskHandle, t->getHandle());
            return -2;
        }

        LOG("Status %d entry %08x", reply->status, reply->entryPoint);

        linkerEntry = reply->entryPoint;
        return reply->status;
    } else {
        return err;
    }

    return 0;
}
