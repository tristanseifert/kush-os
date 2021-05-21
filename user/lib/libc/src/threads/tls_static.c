/**
 * Thread-local implementation for statically linked executables
 */
#include "tls.h"
#include "thread_info.h"

#include <threads.h>

#include <sys/syscalls.h>

#if defined(__amd64__)
#include <sys/amd64/syscalls.h>
#endif

#include <assert.h>
#include <stdalign.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

 #define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

// align `val` up to the nearest `n` byte boundary, where `n` is a power of 2
#define alignup(val, n) (((val) + (n)-1) & -(n))

/**
 * Since we can't currently read out program headers, we have to make an assumption about the
 * alignment of the thread-local data section. In this case, allocate a variable and force it to
 * be 8-byte aligned, so we can assume that's the alignment.
 *
 * This is really a massive fucking hack and we need to figure out how to properly read the
 * program headers for this info in the future, but there also aren't really going to be very many
 * statically linked binaries. It might just be worth going the macOS route and not really
 * supporting static executables. We'll still need it for rootsrv, but that's a special case we
 * can account for.
 */
static thread_local size_t __attribute__((aligned(8))) gUnusedFlag = 0;

// these are defined via app_static.ld
extern uintptr_t __tls_data_start;
extern uintptr_t __tls_data_size, __tls_bss_size, __tls_size;

static void *AllocTls();
static void SetCurrentTlsBase(void *ptr);

/**
 * Structure allocated for a thread local storage in static binaries.
 *
 * The actual TLS data for the executable precedes this block in memory.
 */
typedef struct tls_block_static {
    /// self ptr (used when dereferencing)
    struct tls_block_static *self;

    /// C library thread structure
    uthread_t *thread;
} tls_block_static_t;

/**
 * Invoked to initialize TLS for the main thread.
 */
LIBC_INTERNAL void __libc_tls_main_init() {
    (void) gUnusedFlag;

    // this works the same as for other threads :D
    __libc_tls_init();
}

/**
 * Invoked when a thread is launched to set up its thread-local storage array.
 */
LIBC_INTERNAL void __libc_tls_init() {
    // allocate a TLS
    void *base = AllocTls();
    if(!base) return;

    // store base ptr thread info block
    uthread_t *thrd = thrd_current();
    thrd->tls.s.base = base;
}

/**
 * Tears down a thread's TLS structure.
 */
LIBC_INTERNAL void __libc_tls_fini() {
    uthread_t *thrd = thrd_current();

    // zero out the pointer
    SetCurrentTlsBase(NULL);

    // release the structure
    if(thrd->tls.s.base) {
        free(thrd->tls.s.base);
    }
}

/**
 * Allocates a thread local structure
 *
 * @return Base address of the TLS region (as allocated) or NULL if there is no TLS region
 */
static void *AllocTls() {
    uthread_t *thrd = thrd_current();

    int err;
    void *base;

    // how much thread local storage do we need?
    const size_t tlsSize = (const size_t) &__tls_size;
    if(!tlsSize) return NULL;

    // allocate the memory and ensure it's zeroed
    const size_t tlsAlignment = 0x8; // TODO: read from ELF
    size_t allocAlign = max(tlsAlignment, alignof(tls_block_static_t));
    size_t allocSize = alignup(tlsSize, allocAlign) + sizeof(tls_block_static_t);

    err = posix_memalign(&base, allocAlign, allocSize);
    assert(!err);

    memset(base, 0, allocAlign);

    // set up pointers
    tls_block_static_t *info = (tls_block_static_t *) (((uint8_t *) base) + alignup(tlsSize, allocAlign));
    uint8_t *tls = ((uint8_t *) info) - alignup(tlsSize, tlsAlignment);

    memset(info, 0, sizeof *info);

    info->self = info;
    info->thread = thrd;

    // copy the data
    const size_t tdataSize = (const size_t) &__tls_data_size;

    if(tdataSize) {
        void *from = (void *) &__tls_data_start;
        memcpy(tls, from, tdataSize);
    }

    // update the thread-local pointer
    SetCurrentTlsBase(info);

    thrd->tls.s.base = base;
    thrd->tls.s.length = allocSize;
    thrd->tls.s.tlsRegionLength = alignup(tlsSize, tlsAlignment);

    return base;
}


/**
 * Updates the base pointer of the current thread's thread-local information structure.
 */
static void SetCurrentTlsBase(void *ptr) {
    int err;

#if defined(__amd64__)
    err = Amd64SetThreadLocalBase(SYS_ARCH_AMD64_TLS_FS, (const uintptr_t) ptr);
    assert(!err);
#else
#error Please tell me how to set a thread's TLS base.
#endif
}
