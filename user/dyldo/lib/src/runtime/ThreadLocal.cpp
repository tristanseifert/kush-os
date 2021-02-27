#include "ThreadLocal.h"
#include "Library.h"
#include "Linker.h"
#include "link/SymbolMap.h"

#include <algorithm>
#include <cstring>
#include <new>

#include <malloc.h>
#include <sys/syscalls.h>
#if defined(__i386__)
#include <sys/x86/syscalls.h>
#endif

using namespace dyldo;

bool ThreadLocal::gLogAllocations = false;

/**
 * Exported function to allow the C library to have us do the entire TLS allocation for the current
 * thread.
 *
 * This is nice because the C library now doesn't have to care about the precise implementation
 * of it. The downside here is that allocations are made from our heap rather than the
 * application's, but for typical TLS sizes, even with hundreds of threads, we shoudln't hit even
 * the most restrictive (currently, 16M on x86) dynamic linker heap limit.
 *
 * @return Base address of the TLS; there are many fields in this, but the only one the C library
 * must know about is the base pointer at offset 0.
 */
void *__dyldo_setup_tls() {
    auto tl = Linker::the()->getTls();
    return tl->setUp();
}

/**
 * Releases the memory allocated for the current thread's thread-local storage.
 */
void __dyldo_teardown_tls() {
    Linker::the()->getTls()->tearDown();
}


/**
 * Registers the thread-local info interface.
 */
ThreadLocal::ThreadLocal() {
    // set up the library TLS allocation map
    int err = hashmap_create(4, &this->libRegions);
    if(err) {
        Linker::Abort("%s failed: %d", "hashmap_create", err);
    }

    // register symbols
    Linker::the()->map->addLinkerExport("__dyldo_setup_tls",
            reinterpret_cast<void *>(&__dyldo_setup_tls), 0);
    Linker::the()->map->addLinkerExport("__dyldo_teardown_tls",
            reinterpret_cast<void *>(&__dyldo_teardown_tls), 0);
}

/**
 * Sets the size of the thread-local region requested by the main executable.
 */
void ThreadLocal::setExecTlsInfo(const size_t size, const std::span<std::byte> &tdata) {
    if(gLogAllocations) {
        Linker::Trace("exec: .tdata %u TLS total %u", tdata.size(), size);
    }

    this->totalExecSize = size;

    if(!tdata.empty()) {
        this->tdata = tdata;
    }
}

/**
 * Sets a thread-local reservation for a shared library.
 */
void ThreadLocal::setLibTlsInfo(const size_t size, const std::span<std::byte> &tdata,
        Library *library) {
    // build the info struct
    auto region = new LibTlsRegion(library);
    region->length = size;
    region->tdata = tdata;

    region->offset = this->nextSharedOffset - static_cast<off_t>(size);

    // update offset for next allocation
    this->totalSharedSize += size;
    this->nextSharedOffset -= size;

    if(gLogAllocations) {
        Linker::Trace("lib '%s': .tdata %u TLS total %u off %d", library->soname,
                tdata.size(), size, region->offset);
    }

    // register the region
    int err = hashmap_put(&this->libRegions, library->path, strlen(library->path), region);
    if(err) {
        Linker::Abort("%s failed: %d", "hashmap_put", err);
    }
}

/**
 * Find the TLS offset for the given library.
 *
 * @return TLS offset, or 0 in case of error.
 */
off_t ThreadLocal::getLibTlsOffset(Library *library) {
    // look up by its name
    auto el = hashmap_get(&this->libRegions, library->path, strlen(library->path));
    if(!el) {
        return 0;
    }

    // return the offset
    auto region = reinterpret_cast<const LibTlsRegion *>(el);
    return region->offset;
}

/**
 * Set up the calling thread's thread-local storage. The template data is copied into it, and if
 * required, the thread's architectural state is updated.
 *
 * @return Memory address of the base of the thread structure.
 */
void *ThreadLocal::setUp() {
    // figure out alignment
    const auto alignment = std::max(alignof(uintptr_t), alignof(TlsBlock));

    // actual required TLS space (for executable)
    const auto tlsActualSize = (((this->totalExecSize + alignment - 1) / alignment) * alignment);
    // actual required TLS space (for shared libraries)
    const auto tlsSharedActualSize = (((this->totalSharedSize + alignment - 1) / alignment) * alignment);

    // how much TLS space is to be allocated
    const auto tlsSize = std::max(kTlsMinSize, tlsActualSize + tlsSharedActualSize);
    // size of final allocation
    const auto size = tlsSize + sizeof(TlsBlock);

    if(gLogAllocations) {
        Linker::Trace("Total TLS size: %u alloc %u (exec %u lib %u)", tlsSize, size, tlsActualSize, tlsSharedActualSize);
    }

    // allocate the region and zero it
    void *base = nullptr;
    int err = posix_memalign(&base, alignment, size);
    if(err) {
        Linker::Abort("%s failed: %d", "posix_memalign", err);
    }

    memset(base, 0, size);

    // get location of the structures
    const auto tbBase = reinterpret_cast<uintptr_t>(base) + tlsSize;
    TlsBlock *tb = new(reinterpret_cast<void *>(tbBase)) TlsBlock;
    tb->self = tb;
    tb->memBase = base;

    // copy in the TLS defaults (for the executable)
    const auto tlsBase = tbBase - tlsActualSize;
    auto tls = reinterpret_cast<void *>(tlsBase);

    if(!this->tdata.empty()) {
        memcpy(tls, this->tdata.data(), this->tdata.size());
    }

    // copy in TLS defaults (for all shared libraries)
    hashmap_iterate(&this->libRegions, [](void *ctx, void *_region) -> int {
        // get offset from the TLS
        auto region = reinterpret_cast<const LibTlsRegion *>(_region);

        const auto baseAddr = reinterpret_cast<uintptr_t>(ctx) + region->offset;
        auto base = reinterpret_cast<void *>(baseAddr);

        if(!region->tdata.empty()) {
            memcpy(base, region->tdata.data(), region->tdata.size());
        }

        return 1;
    }, tls);

    // update the thread's arch state and return
    tb->tlsBase = tls;
    this->updateThreadArchState(tb);

    return tb;
}

/**
 * Tears down the TLS region.
 */
void ThreadLocal::tearDown() {
    // get the base address of the TlsBlock
    uintptr_t tlsBlockBase = 0;
#if defined(__i386__)
    asm volatile("mov   %%gs:0x00, %0" : "=r" (tlsBlockBase));
#else
#error Update ThreadLocal for current arch
#endif

    // release its memory
    auto tls = reinterpret_cast<TlsBlock *>(tlsBlockBase);
    Linker::Trace("deallocating tls: %p (%p)", tls, tls->memBase);

    free(tls->tlsBase);

    // clear arch state (so we don't refer to invalid memory)
    this->updateThreadArchState(nullptr);
}

/**
 * Updates the thread's architectural state to point to the new userspace thread-local structure.
 */
void ThreadLocal::updateThreadArchState(TlsBlock *tls) {
#if defined(__i386__)
    int err = X86SetThreadLocalBase(reinterpret_cast<uintptr_t>(tls));
    if(err) {
        Linker::Abort("%s failed: %d", "X86SetThreadLocalBase", err);
    }
#else
#error Update ThreadLocal for current arch
#endif
}
