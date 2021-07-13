#include "EventSubmitter.h"
#include "Client_WindowServer.hpp"

#include "Log.h"

#include <threads.h>
#include <rpc/dispensary.h>
#include <rpc/rt/ClientPortRpcStream.h>

EventSubmitter *EventSubmitter::gShared{nullptr};

/**
 * Returns the shared event submitter instance, allocating it if needed.
 */
EventSubmitter *EventSubmitter::the() {
    static once_flag flag = ONCE_FLAG_INIT;
    call_once(&flag, []() {
        gShared = new EventSubmitter;
    });

    return gShared;
}



/**
 * Submits a mouse event to the window server. If the RPC connection is not valid or otherwise
 * unavailable, and we cannot reestablish it, the event is discarded.
 */
void EventSubmitter::submitMouseEvent(const uintptr_t buttons,
        const std::tuple<int, int, int> &deltas) {
    const auto [dX, dY, dZ] = deltas;

    // ensure RPC connection
    if(!this->rpc) {
        if(!this->connect()) {
            Warn("Discarding mouse event: (%d, %d, %d, %08x)", dX, dY, dZ, buttons);
            return;
        }
    }

    // submit the event
    // XXX: we need to flip the Y coordinate for some reason?
    rpc->SubmitMouseEvent(buttons, dX, -dY, dZ);
}

/**
 * Submits a keyboard event to the window server. We've already translated the key code from the
 * PS/2 specific scancode set to the generic scancode set the window server expects.
 */
void EventSubmitter::submitKeyEvent(const uint32_t key, const bool isMake) {
    // ensure RPC connection
    if(!this->rpc) {
        if(!this->connect()) {
            Warn("Discarding key event: (%08x, %5s)", key, isMake ? "make" : "break");
            return;
        }
    }

    rpc->SubmitKeyEvent(key, !isMake);
}


/**
 * Attempts to establish an RPC connection to the window server.
 */
bool EventSubmitter::connect() {
    int err;
    uintptr_t port;

    // look up the window server port
    err = LookupService(kWindowServerPortName.data(), &port);
    if(err != 1) {
        // an error occurred
        if(err > 0) {
            Abort("%s failed: %d", "LookupService", err);
        } 
        // the port hasn't been registered yet
        else {
            return false;
        }
    }

    // create RPC connection
    auto stream = std::make_shared<rpc::rt::ClientPortRpcStream>(port);
    this->rpc = std::make_unique<rpc::WindowServerClient>(stream);

    return true;
}
