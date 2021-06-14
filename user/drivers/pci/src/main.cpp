#include <cstring>
#include <memory>
#include <thread>
#include <vector>

#include <sys/syscalls.h>

#include "bus/RootBridge.h"
#include "bus/pcie/PciExpressBus.h"
#include "Log.h"

const char *gLogTag = "pci";

/**
 * Entry point for the PCI driver. It's responsible for scanning all PCI busses and providing
 * interrupt and other resource management.
 */
int main(const int argc, const char **argv) {
    std::vector<std::shared_ptr<PciExpressBus>> pcie;

    Success("pcisrv starting");

    // initialize instances for each additional argument
    for(size_t i = 1; i < argc; i++) {
        const std::string_view path(argv[i]);

        // is it PCI Express?
        if(path.find("PciExpress") != std::string_view::npos) {
            auto bus = std::make_shared<PciExpressBus>(path);
            pcie.push_back(std::move(bus));
        }
    }

    // scan for devices on all busses
    Trace("Beginning PCI device scan...");
    size_t devices{0};
    for(const auto &bus : pcie) {
        bus->scan();
        devices += bus->getDevices().size();
    }
    Success("Completed PCI device scan. Found %lu devices", devices);

    // TODO: start message loop
    while(1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(420));
    }
}
