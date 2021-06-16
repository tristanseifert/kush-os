#include <cstring>
#include <memory>
#include <thread>
#include <vector>

#include "BusRegistry.h"
#include "Log.h"
#include "rpc/Server.h"
#include "bus/RootBridge.h"
#include "bus/pcie/PciExpressBus.h"

const char *gLogTag = "pci";

/**
 * Entry point for the PCI driver. It's responsible for scanning all PCI busses and providing
 * interrupt and other resource management.
 */
int main(const int argc, const char **argv) {
    Success("pcisrv starting");
    BusRegistry::init();
    RpcServer::init();

    // initialize instances for each additional argument
    for(size_t i = 1; i < argc; i++) {
        const std::string_view path(argv[i]);

        // is it PCI Express?
        if(path.find("PciExpress") != std::string_view::npos) {
            auto bus = std::make_shared<PciExpressBus>(path);
            BusRegistry::the()->add(std::move(bus));
        }
    }

    // scan for devices on all busses
    Trace("Beginning PCI device scan...");
    const auto devices = BusRegistry::the()->scanAll();
    Success("Completed PCI device scan. Found %lu devices", devices);

    // start the message loop
    RpcServer::the()->run();
    Abort("RpcServer returned!");
}
