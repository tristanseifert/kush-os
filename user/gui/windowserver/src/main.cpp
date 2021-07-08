#include "Log.h"
#include "compositor/Compositor.h"
#include "rpc/RpcServer.h"

#include <DriverSupport/gfx/Display.h>

const char *gLogTag = "windowserver";

/**
 * Entry point for the window server.
 *
 * The first argument is always the root display on which the primary desktop goes.
 */
int main(const int argc, const char **argv) {
    int err;
    if(argc != 2) {
        Abort("You must specify the forest path of a display.");
    }
    Success("WindowServer starting (root display: %s)", argv[1]);

    // open the display client and create a compositor
    std::shared_ptr<DriverSupport::gfx::Display> display;
    err = DriverSupport::gfx::Display::Alloc(argv[1], display);
    if(err) {
        Abort("Failed to initialize display (%s): %d", argv[1], err);
    }

    auto compositor = std::make_shared<Compositor>(display);

    // create and start RPC server
    RpcServer server(compositor);
    err = server.run();

    Trace("Run loop returned: %d", err);

    return err;
}
