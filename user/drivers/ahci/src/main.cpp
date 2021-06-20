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
    int err;

    if(argc < 2) {
        Abort("You must specify at least one forest path of a device to attach to.");
    }

    ControllerRegistry::init();

    // create the devices
    for(size_t i = 1; i < argc; i++) {
        std::shared_ptr<libpci::Device> pciDev;
        err = libpci::Device::Alloc(argv[i], pciDev);
        if(err) {
            Abort("Failed to allocate PCIe device for '%s': %d", argv[i], err);
        }

        auto controller = std::make_shared<Controller>(pciDev);

        ControllerRegistry::the()->addController(controller);
    }

    // detect attached disks/drives
    Trace("Starting device probe");
    for(const auto &controller : ControllerRegistry::the()->get()) {
        controller->probe();
    }

    // receive messages from driverman
    while(1) {
        ThreadUsleep(1000 * 500);
    }

    // clean up
    ControllerRegistry::deinit();
    return 0;
}
