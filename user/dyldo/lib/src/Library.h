#ifndef DYLDO_LIBRARY_H
#define DYLDO_LIBRARY_H

#include <cstdint>
#include <cstring>
#include <list>

#include "struct/StringAllocator.h"

namespace dyldo {
class ElfLibReader;

/**
 * Information for a loaded library. This consists of the virtual address at which it was
 * loaded, file paths, and so on.
 *
 * The global symbol table will link back to these objects, which the global linker object keeps
 * track of.
 */
struct Library {
    /// library soname
    const char * _Nonnull soname;
    /// filesystem path the library was loaded from
    const char * _Nonnull path;

    /// base address
    uintptr_t base = 0;
    /// ELF reader for the library (during loading)
    ElfLibReader * _Nullable reader = nullptr;

    /// virtual memory base and length
    uintptr_t vmBase = 0, vmLength = 0;

    /// initialization functions exported by this object
    std::list<void(*)(void)> initFuncs;
    /// all termination functions associated with this object
    std::list<void(*)(void)> finiFuncs;

    /// string allocator
    StringAllocator strings;
};
}

#endif
