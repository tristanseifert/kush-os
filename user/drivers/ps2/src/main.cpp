#include <cstddef>
#include <cstring>
#include <memory>
#include <thread>
#include <span>
#include <vector>

#include <malloc.h>
#include <driver/base85.h>
#include <sys/syscalls.h>

#include "Ps2Controller.h"
#include "Log.h"

const char *gLogTag = "ps2";

/// global PS2 controller object
std::shared_ptr<Ps2Controller> gController;

/**
 * Entry point for the PS/2 controller: there can only ever be one of these in a system.
 */
int main(const int argc, const char **argv) {
    // decode the first argument as base85
    if(argc != 2) {
        Abort("must have exactly one argument");
    }

    const auto inChars = strlen(argv[1]);
    void *data = malloc(inChars);
    if(!data) Abort("out of memory");

    void *end = b85tobin(data, argv[1]);
    if(!end) Abort("failed to convert base85");

    const auto binBytes = reinterpret_cast<uintptr_t>(end) - reinterpret_cast<uintptr_t>(data);

    // initialize a controller instance
    std::span<std::byte> aux(reinterpret_cast<std::byte *>(data), binBytes);
    gController = std::make_shared<Ps2Controller>(aux);

    // clean up
    free(data);

    // TODO: start message loop
    while(1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(420));
    }

    // clean up
    gController = nullptr;
}
