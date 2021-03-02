#include "MessageLoop.h"
#include "Log.h"

#include <malloc.h>
#include <system_error>

#include <rpc/dispensary.h>
#include <rpc/RpcPacket.hpp>

MessageLoop *MessageLoop::gShared = nullptr;

/**
 * Sets up the message loop:
 *
 * Create the port used to receive requests and register with the dispensary.
 */
MessageLoop::MessageLoop() {
    int err;

    // allocate receive buffer
    err = posix_memalign(&this->rxBuf, 16, kRxBufSize);
    if(err) {
        throw std::system_error(err, std::generic_category(), "posix_memalign");
    }
    memset(this->rxBuf, 0, kRxBufSize);

    // allocate port
    err = PortCreate(&this->port);
    if(err) {
        throw std::system_error(err, std::generic_category(), "PortCreate");
    }

    // register
    err = RegisterService(kServiceName, this->port);
    if(err) {
        throw std::system_error(err, std::generic_category(), "RegisterService");
    }

    this->shouldRun = true;
}

/**
 * Releases message loop resources.
 */
MessageLoop::~MessageLoop() {
    this->shouldRun = false;

    // TODO: unregister service
    PortDestroy(this->port);
    free(this->rxBuf);
}

/**
 * Processes messages forever.
 */
void MessageLoop::run() {
    int err;

    Trace("Entering message loop");

    while(this->shouldRun) {
        // try to receive message
        auto buf = reinterpret_cast<MessageHeader_t *>(this->rxBuf);
        err = PortReceive(this->port, buf, kRxBufSize, UINTPTR_MAX);

        if(err < 0) {
            throw std::system_error(err, std::generic_category(), "PortReceive");
        }

        // process the message
        Trace("Received message %u bytes", buf->receivedBytes);
    }
}
