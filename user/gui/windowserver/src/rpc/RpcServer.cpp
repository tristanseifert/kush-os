#include "RpcServer.h"
#include "Log.h"

#include <rpc/rt/ServerPortRpcStream.h>

/**
 * Initializes the RPC server. A listening port will be opened and registered.
 */
RpcServer::RpcServer(const std::shared_ptr<Compositor> &comp) :
    rpc::WindowServerServer(std::make_shared<rpc::rt::ServerPortRpcStream>(kPortName)) {
    this->addCompositor(comp);
}

/**
 * Adds a compositor to the internal compositors list.
 */
void RpcServer::addCompositor(const std::shared_ptr<Compositor> &comp) {
    REQUIRE(this->comps.empty(), "No support for multiple compositors yet");
}



/**
 * Handles a received key event.
 */
void RpcServer::implSubmitKeyEvent(uint32_t scancode, bool release) {
    Trace("Key event: %5s %08x", release ? "break" : "make", scancode);
}


/**
 * Handles a received mouse movement event.
 */
void RpcServer::implSubmitMouseEvent(uint32_t buttons, int32_t dX, int32_t dY, int32_t dZ) {
    Trace("Mouse event: buttons %08x offsets %4d %4d %4d", buttons, dX, dY, dZ);
}
