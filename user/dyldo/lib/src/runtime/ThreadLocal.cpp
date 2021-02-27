#include "ThreadLocal.h"
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

/**
 * Exported function to allow C library to get information on the TLS size.
 */
const size_t __dyldo_get_tls_info(void **outData, size_t *outDataLen) {
    // get the shared instance from the linker
    auto tl = Linker::the()->getTls();

    // get info out of it
    if(outData) {
        *outData = tl->tdata.data();
    }
    if(outDataLen) {
        *outDataLen = tl->tdata.size();
    }

    return tl->totalSize;
}

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
    // register symbols
    Linker::the()->map->addLinkerExport("__dyldo_get_tls_info",
            reinterpret_cast<void *>(&__dyldo_get_tls_info), 0);
    Linker::the()->map->addLinkerExport("__dyldo_setup_tls",
            reinterpret_cast<void *>(&__dyldo_setup_tls), 0);
    Linker::the()->map->addLinkerExport("__dyldo_teardown_tls",
            reinterpret_cast<void *>(&__dyldo_teardown_tls), 0);
}

/**
 * Sets the size of the thread-local region requested by the main executable.
 */
void ThreadLocal::setExecTlsInfo(const size_t size, const std::span<std::byte> &tdata) {
    Linker::Trace("exec: .tdata %u TLS total %u", tdata.size(), size);

    this->totalSize = size;

    if(!tdata.empty()) {
        this->tdata = tdata;
    }
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
    // actual required TLS space
    const auto tlsActualSize = (((this->totalSize + alignment - 1) / alignment) * alignment);
    // how much TLS space is allocated
    const auto tlsSize = std::max(kTlsMinSize, tlsActualSize);
    // size of final allocation
    const auto size = tlsSize + sizeof(TlsBlock);

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

    // copy in the TLS defaults
    const auto tlsBase = tbBase - tlsActualSize;
    auto tls = reinterpret_cast<void *>(tlsBase);

    if(!this->tdata.empty()) {
        memcpy(tls, this->tdata.data(), this->tdata.size());
    }

    tb->tlsBase = tls;

    // update the thread's arch state and return
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
