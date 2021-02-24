#include "Prelink.h"
#include "dylib/Library.h"
#include "dylib/Registry.h"

#include "log.h"

#include <array>
#include <cstdio>
#include <memory>
#include <iostream>
#include <string>
#include <vector>

#include <sys/elf.h>
#include <sys/syscalls.h>

using namespace prelink;
using namespace std::literals;

/**
 * Absolute paths of libraries that need to be preloaded.
 */
constexpr static const size_t kNumPrelinkLibs = 7;
static const std::array<std::string, kNumPrelinkLibs> kPrelinkLibraryPaths = {
    "/lib/libc.so",
    "/lib/libc++abi.so.1",
    "/lib/libc++.so.1",
    "/lib/libunwind.so.1",
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
    std::vector<std::pair<uintptr_t, std::shared_ptr<dylib::Library>>> libs;
    libs.reserve(kNumPrelinkLibs);

    for(auto &path : kPrelinkLibraryPaths) {
        // try to open the library and reserve its VM space
        L("Opening library '{}' (base {:x})", path, vmBase);

        auto library = dylib::Library::loadFile(path);
        if(!library) {
            L("Failed to load library '{}'", path);
            std::terminate();
        }

        // load the library's pages and flush file caches
        if(!library->allocateProgbitsVm(vmBase)) {
            L("Failed to allocate progbits section for library '{}'", path);
            std::terminate();
        }

        library->closeFile();

        // register the library
        dylib::Registry::add(library, path);
        libs.emplace_back(std::make_pair(vmBase, library));

        // advance the VM base past this library. round it UP to the nearest 1MB bound
        vmBase += library->getVmRequirements();

        if(vmBase & ~0xFFFFF) {
            vmBase += 0x100000 - (vmBase & 0xFFFFF);
        }
    }

    // iterate over each library and perform relocations
    for(auto &[base, library] : libs) {
        L("Library {} at {:x}", library->getSoname(), base);

        if(!library->resolveImports(libs)) {
            L("Unresolved imports in {}!", library->getSoname());
            std::terminate();
        }
    }

    // ensure there's no unresolved symbols in any libraries
    for(auto &[base, library] : libs) {
        if(library->hasUnresolvedRelos()) {
            L("Unresolved relocations in {}!", library->getSoname());
            std::terminate();
        }
    }
}
