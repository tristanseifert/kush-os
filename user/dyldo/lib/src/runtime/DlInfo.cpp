#include "DlInfo.h"
#include "Library.h"
#include "Linker.h"
#include "link/SymbolMap.h"

#include "elf/ElfExecReader.h"
#include "elf/ElfLibReader.h"

#include <link.h>

using namespace dyldo;

// Forward to the DlInfo member function
int __dyldo_dl_iterate_phdr(int (*callback)(struct dl_phdr_info *info, size_t size, void *ctx), void *ctx) {
    auto info = Linker::the()->getDlInfo();
    return info->iterateObjs(callback, ctx);
}
void *__dyldo_dlsym(void *handle, const char *name) {
    auto info = Linker::the()->getDlInfo();
    return info->resolve(handle, name);
}

const char *__dyldo_dlerror() {
    Linker::Abort("%s unimplemented", "dlerror");
    return nullptr;
}


/**
 * Initializes the dynamic linker info runtime API.
 */
DlInfo::DlInfo() {
    // register symbols
    Linker::the()->map->addLinkerExport("dl_iterate_phdr",
            reinterpret_cast<void *>(&__dyldo_dl_iterate_phdr), 0);
    Linker::the()->map->addLinkerExport("dlsym", reinterpret_cast<void *>(&__dyldo_dlsym), 0);
    Linker::the()->map->addLinkerExport("dlerror", reinterpret_cast<void *>(&__dyldo_dlerror), 0);
}

/**
 * Iterate over the objects array.
 */
int DlInfo::iterateObjs(int (*callback)(struct dl_phdr_info *, size_t, void*), void *ctx) {
    int last = 0;

    // invoke callback for each object
    for(const auto &info : this->loadedObjs) {
        // build up info
        struct dl_phdr_info dlInfo;
        memset(&dlInfo, 0, sizeof(dlInfo));

        // we've got a shared library
        if(info.library) {
            const auto lib = info.library;
            dlInfo.dlpi_addr = lib->base;
        }
        // otherwise, it's an executable
        else {
            dlInfo.dlpi_addr = 0;
        }

        dlInfo.dlpi_name = info.path;
        dlInfo.dlpi_phdr = info.phdrs.data();
        dlInfo.dlpi_phnum = info.phdrs.size();

        // invoke callback
        last = callback(&dlInfo, sizeof(dlInfo), ctx);
        Linker::Trace("Iterating object '%s' base $%08x (phdrs at %p %u) ret %d", dlInfo.dlpi_name, dlInfo.dlpi_addr, dlInfo.dlpi_phdr, dlInfo.dlpi_phnum, last);

        if(last) goto beach;
    }

beach:;
    return last;
}

/**
 * Resolve a symbol.
 */
void *DlInfo::resolve(void *handle, const char *name) {
    Linker::Abort("dlsym(%p, %s) unimplemented", handle, name);
}

/**
 * Store information about the main executable.
 */
void DlInfo::loadedExec(ElfExecReader *elf, const char *path) {
    Object info;

    info.path = path;
    elf->getVmPhdrs(info.phdrs);

    this->loadedObjs.push_front(info);
}

/**
 * Store information about a loaded library.
 */
void DlInfo::loadedLib(ElfLibReader *elf, Library *lib) {
    Object info;

    info.path = lib->path;
    info.library = lib;

    elf->getVmPhdrs(info.phdrs);

    this->loadedObjs.push_back(info);
}
