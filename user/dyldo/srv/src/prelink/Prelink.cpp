#include "Prelink.h"
#include "dylib/Library.h"

#include "log.h"

#include <array>
#include <cstdio>
#include <string>
#include <iostream>

#include <sys/elf.h>
#include <sys/syscalls.h>

using namespace prelink;
using namespace std::literals;

/**
 * Absolute paths of libraries that need to be preloaded.
 */
static const std::array<std::string, 6> kPrelinkLibraryPaths = {
    "/lib/libc.so",
    "/lib/libc++abi.so.1",
    "/lib/libc++.so.1",
    "/lib/libsystem.so",
    "/lib/librpc.so",
    "/lib/libdyldo.so",
};

/**
 * Performs initialization of the prelink stage. This does the following:
 *
 * - Open each library in sequence.
 *   - Validate the header to ensure it's a dynamic library and meets the platform requirements.
 *   - Create a library object to represent this library. This will hold all information, such as
 *     exported symbols, that other callers may look up later.
 *   - Reserve virtual memory space for all loadable segments (including those without an actual
 *     file backed content, like .bss) in the high library area.
 *   - Extract exported symbols and their library-relative addresses.
 *   - Allocate virtual memory for all segments with file backed contents. (So, sections like .bss
 *     are ignored at this stage.)
 *   - Load the segments' data from the file into the virtual memory regions.
 * - Resolve unknown symbols in each library.
 *   - Process all relocations listed in the ELF file. At this stage, the libraries are ready to be
 *     loaded into processes' address spaces as is.
 *   - Restore the protection level on all segments we loaded. Executable segments are marked as
 *     R+X only. Data segments are marked as read-only, since we're only going to be using them as
 *     a "template" when mapping them into programs.
 *
 * At this point, we're done pre-linking. When a new task is created, we'll map the fixed, read-
 * only segments into it at the correct address space. Then, we'll allocate virtual mappings for
 * the writable data segments, and copy into it the libraries' data segments.
 */
void prelink::Load() {
    /// high load address: XXX maybe better options?
    uintptr_t vmBase = 0xB0000000;

    // load each of the libraries
    for(auto &path : kPrelinkLibraryPaths) {
        // try to open the library and reserve its VM space
        L("Opening library '{}' (base {:x})", path, vmBase);

        auto library = dylib::Library::loadFile(path);
        if(!library) {
            L("Failed to load library '{}'", path);
            std::terminate();
        }

        vmBase += library->getVmRequirements();
    }
}
