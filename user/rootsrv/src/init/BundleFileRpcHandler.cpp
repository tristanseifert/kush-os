#include "BundleFileRpcHandler.h"
#include "Bundle.h"

#include <system_error>

#include "log.h"
#include "dispensary/Dispensary.h"

#include <sys/syscalls.h>

#include <rpc/RpcPacket.hpp>
#include <rpc/FileIO.hpp>

using namespace std::literals;
using namespace init;

// declare the constants for the handler
const std::string_view BundleFileRpcHandler::kPortName = "me.blraaz.rpc.rootsrv.initfileio"sv;
// shared instance of the bundle file IO handler
BundleFileRpcHandler *BundleFileRpcHandler::gShared = nullptr;

/**
 * Initializes the bundle RPC handler. We'll allocate a port and start the work loop.
 */
BundleFileRpcHandler::BundleFileRpcHandler(std::shared_ptr<Bundle> &_bundle) : bundle(_bundle) {
    int err;

    // set up the port
    err = PortCreate(&this->portHandle);
    if(err) {
        throw std::system_error(err, std::generic_category(), "PortCreate");
    }

    LOG("Bundle file IO port: $%08x'h", this->portHandle);

    // create the thread next
    this->run = true;
    this->worker = std::make_unique<std::thread>(&BundleFileRpcHandler::main, this);

    // lastly, register the port
    dispensary::RegisterPort(kPortName, this->portHandle);
}

/**
 * Main loop for the init bundle file RPC service
 */
void BundleFileRpcHandler::main() {
    int err;

    ThreadSetName(0, "rpc: init file ep");

    // allocate receive buffers for messages
    void *rxBuf = nullptr;
    err = posix_memalign(&rxBuf, 16, kMaxMsgLen);
    REQUIRE(!err, "failed to allocate message rx buffer: %d", err);

    // process messages
    while(this->run) {
        // clear out any previous messages
        memset(rxBuf, 0, kMaxMsgLen);

        // read from the port
        struct MessageHeader *msg = (struct MessageHeader *) rxBuf;
        err = PortReceive(this->portHandle, msg, kMaxMsgLen, UINTPTR_MAX);

        if(err > 0) {
            // read out the type
            if(msg->receivedBytes < sizeof(rpc::RpcPacket)) {
                LOG("Port $%08x'h received too small message (%u)", this->portHandle,
                        msg->receivedBytes);
                continue;
            }

            const auto packet = reinterpret_cast<rpc::RpcPacket *>(msg->data);

            // invoke the appropriate handler
            switch(packet->type) {
                //case static_cast<uint32_t>(rpc::RootSrvTaskEpType::kTaskCreate):
                //    break;

                default:
                    LOG("Init file RPC invalid msg type: $%08x", packet->type);
                    break;
            }
        }
        // an error occurred
        else {
            LOG("Port rx error: %d", err);
        }
    }

    // clean up
    free(rxBuf);
}

