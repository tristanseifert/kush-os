#include <cstddef>
#include <vector>

#include <libpci/UserClient.h>

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

    // create the devices
    for(size_t i = 1; i < argc; i++) {
        auto dev = std::make_shared<libpci::Device>(argv[i]);

        Trace("Device %lu: %s (%p) vid %04x pid %04x, class %02x:%02x", i-1, argv[i], dev.get(),
                dev->getVid(), dev->getPid(), dev->getClassId(), dev->getSubclassId());
    }

    // start em
    return 0;
}
