#include <cstddef>
#include <vector>

#include <libpci/UserClient.h>

#include "Controller.h"
#include "ControllerRegistry.h"
#include "Log.h"

const char *gLogTag = "ahci";

/**
 * Entry point for the AHCI server. Every command line argument is the forest path of a device
 * that should be started.
 */
int main(const int argc, const char **argv) {
    if(argc < 2) {
        Abort("You must specify at least one forest path of a device to attach to.");
    }

    ControllerRegistry::init();

    // create the devices
    for(size_t i = 1; i < argc; i++) {
        auto pciDev = std::make_shared<libpci::Device>(argv[i]);
        auto controller = std::make_shared<Controller>(pciDev);

        ControllerRegistry::the()->addController(controller);
    }

    // start em

    // receive messages from driverman
    return 0;
}
