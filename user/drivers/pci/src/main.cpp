#include <cstring>
#include <memory>
#include <thread>
#include <vector>

#include <malloc.h>
#include <driver/base85.h>
#include <sys/syscalls.h>

#include "bus/PciConfig.h"
#include "bus/RootBridge.h"
#include "Log.h"

const char *gLogTag = "pci";

/// all PCI root bridges in the system
std::vector<std::shared_ptr<RootBridge>> roots;

/**
 * Entry point for the driver manager
 */
int main(const int argc, const char **argv) {
    Success("pcisrv starting (%u args)", argc);

    // set up some stuff
    PciConfig::init();

    // initialize instances for each additional argument
    for(size_t i = 1; i < argc; i++) {
        // convert base85 to binary
        const auto inChars = strlen(argv[i]);
        void *data = malloc(inChars);
        if(!data) Abort("out of memory");

        void *end = b85tobin(data, argv[i]);
        if(!end) Abort("failed to convert base85");

        const auto binBytes = reinterpret_cast<uintptr_t>(end) - reinterpret_cast<uintptr_t>(data);

        // create a bridge from this
        std::span<std::byte> aux(reinterpret_cast<std::byte *>(data), binBytes);

        auto bridge = std::make_shared<RootBridge>(aux);
        roots.emplace_back(std::move(bridge));

        // clean up again
        free(data);
    }

    // TODO: start message loop
    while(1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(420));
    }
}
