/*
 * Roughly based on the original OpenBSD arc4random portability stubs, original copyright notice
 * below:
 *
 * Copyright (c) 1996, David Mazieres <dm@uun.org>
 * Copyright (c) 2008, Damien Miller <djm@openbsd.org>
 * Copyright (c) 2013, Markus Friedl <markus@openbsd.org>
 * Copyright (c) 2014, Theo de Raadt <deraadt@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <sys/syscalls.h>


/**
 * Invoked when we fail to get more random entropy. This terminates the task.
 */
static inline void
_getentropy_fail(int err)
{
    fprintf(stderr, "[libc] failed to get entropy: %d\n", err);
    TaskExit(0, -1);
}

/**
 * Allocates internal random state.
 */
#if defined(__amd64__)
#ifdef LIBC_NOTLS
#define STATE_ADDR                      0x7ff000010000
#else
#define STATE_ADDR                      0x7ff000000000
#endif
#else
#error define arc4random state vm address
#endif

static inline int
_rs_allocate(struct _rs **rsp, struct _rsx **rsxp) {
    int err;

    // memory to allocate; round up to page size
    struct {
        struct _rs rs;
        struct _rsx rsx;
    } *p;

    const size_t pageSz = sysconf(_SC_PAGESIZE);
    const size_t bytes = ((sizeof(*p) + pageSz - 1) / pageSz) * pageSz;

    // perform allocation and map it
    uintptr_t handle;

    err = AllocVirtualAnonRegion(bytes, VM_REGION_RW, &handle);
    if(err) {
        fprintf(stderr, "%s failed: %d\n", "AllocVirtualAnonRegion", err);
        abort();
    }

    err = MapVirtualRegion(handle, STATE_ADDR, bytes, 0);
    if(err) {
        fprintf(stderr, "%s failed: %d\n", "MapVirtualRegion", err);
        abort();
    }

    p = (void *) STATE_ADDR;


    // successfully allocated region
    *rsp = &p->rs;
    *rsxp = &p->rsx;
    return 0;
}

