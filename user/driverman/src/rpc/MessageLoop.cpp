#include "MessageLoop.h"
#include "Log.h"

#include "db/Driver.h"
#include "db/DriverDb.h"

#include <malloc.h>
#include <system_error>

#include <driver/Discovery.hpp>

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

        // read out the packet header
        if(buf->receivedBytes < sizeof(rpc::RpcPacket)) {
            Warn("Received %u bytes (too small)", buf->receivedBytes);
            continue;
        }

        auto packet = reinterpret_cast<rpc::RpcPacket *>(buf->data);
        Trace("Received %u bytes", buf->receivedBytes);

        // invoke the appropriate handler
        switch(packet->type) {
            case libdriver::DeviceDiscovered::kRpcType:
                this->handleDiscover(buf, packet);
                break;

            default:
                Warn("Unknown RPC type $%08x", packet->type);
                break;
        }
    }
}

/**
 * Handles a newly discovered device.
 */
void MessageLoop::handleDiscover(MessageHeader *msg, rpc::RpcPacket *packet) {
    // try to deserialize it
    const auto reqBuf = std::span(reinterpret_cast<std::byte *>(packet->payload),
            msg->receivedBytes - sizeof(rpc::RpcPacket));

    libdriver::DeviceDiscovered disc;
    disc.deserializeFull(reqBuf);

    // find a driver
    Trace("Discovered device: %u match descriptors, %u aux bytes", disc.matches.size(),
            disc.aux.size());

/*    for(auto _match : disc.matches) {
        switch(_match->getRpcType()) {
            case libdriver::DeviceNameMatch::kRpcType: {
                auto match = reinterpret_cast<libdriver::DeviceNameMatch *>(_match);
                Trace("Name match: '%s'", match->name.c_str());
                break;
            }

            default:
                Warn("Unknown match type (obj %p type %08x)", _match, _match->getRpcType());
                break;
        }
    }
    */

    auto driver = DriverDb::the()->findDriver(disc.matches);
    Trace("Found driver: %p path %s", driver.get(), (driver ? driver->getPath().c_str() : "(null)"));

    // failed to find driver
    if(!driver) {
        Warn("Failed to find driver for device!");
    } 
    // create an instance thereof
    else {
        driver->start(disc.aux);
    }
}

