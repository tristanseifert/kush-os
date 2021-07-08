#include "RpcServer.h"
#include "Log.h"

#include <rpc/rt/ServerPortRpcStream.h>

/**
 * Adds a compositor to the internal compositors list.
 */
void RpcServer::addCompositor(const std::shared_ptr<Compositor> &comp) {
    REQUIRE(this->comps.empty(), "No support for multiple compositors yet");
}

/**
 * Message handling loop
 */
int RpcServer::run() {
    // TODO: implement
    return -1;
}
