#include <cstdio>
#include <thread>

#include "log.h"
#include "dylib/Registry.h"
#include "prelink/Prelink.h"

/**
 * Entry point for the dynamic link server
 *
 * We'll load the global runtime libraries (these are what's required for the dynamic linker to
 * function: and are probably used by most, if not all, executables on the platform) and pre-
 * link them at a high base address. Because these libraries will be pre-linked, adding them to a
 * newly launched process is simply a shared memory mapping, as well as an allocation of the
 * appropriate data sections.
 *
 * Next, we'll set up the bootstrap and RPC API ports, which are used again by the dynamic linker
 * runtime in each process. This allows processes to query the cache of loaded objects.
 *
 * After this point, we're really just waiting for RPC requests. Our main purpose beyond this point
 * is to map shared library text segments into processes, and to resolve symbols.
 */
int main(const int argc, const char **argv) {
    // set up environment
    dylib::Registry::init();

    // perform the initial library loading
    prelink::Load();

    // set up the RPC servers

    // enter the RPC request loop 

    while(1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}
