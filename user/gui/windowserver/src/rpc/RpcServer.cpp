#include "RpcServer.h"
#include "compositor/Compositor.h"

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
    this->comps.emplace_back(std::move(comp));
}



/**
 * Handles a received key event.
 */
void RpcServer::implSubmitKeyEvent(uint32_t scancode, bool release) {
    const auto &c = this->comps[0];
    c->handleKeyEvent(scancode, release);
}


/**
 * Handles a received mouse movement event.
 *
 * This pushes the relative movements into the compositor's mouse handler, which is responsible for
 * scaling the input and updating the position of the cursor on screen. It will also handle sending
 * the event to any interested parties.
 */
void RpcServer::implSubmitMouseEvent(uint32_t buttons, int32_t dX, int32_t dY, int32_t dZ) {
    const auto &c = this->comps[0];
    c->handleMouseEvent({dX, dY, dZ}, buttons);
}

