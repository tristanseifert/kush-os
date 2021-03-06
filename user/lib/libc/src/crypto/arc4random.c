/*	$OpenBSD: arc4random.c,v 1.55 2019/03/24 17:56:54 deraadt Exp $	*/

/*
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

/*
 * ChaCha based random number generator for OpenBSD.
 */

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <_libc.h>

#include <sys/syscalls.h>

#define KEYSTREAM_ONLY
#include "chacha_private.h"

#define minimum(a, b) ((a) < (b) ? (a) : (b))

#define KEYSZ	32
#define IVSZ	8
#define BLOCKSZ	64
#define RSBUFSZ	(16*BLOCKSZ)

/* Marked MAP_INHERIT_ZERO, so zero'd out in fork children. */
static struct _rs {
	size_t		rs_have;	/* valid bytes at end of rs_buf */
	size_t		rs_count;	/* bytes till reseed */
} *rs;

/* Maybe be preserved in fork children, if _rs_allocate() decides. */
static struct _rsx {
	chacha_ctx	rs_chacha;	/* chacha context for random keystream */
	unsigned char	rs_buf[RSBUFSZ];	/* keystream blocks */
} *rsx;

/// lock protecting the arc4random machinery
static __attribute__((aligned(sizeof(uintptr_t)))) bool gLock = false;

#define _ARC4_LOCK() {while(__atomic_test_and_set(&gLock, __ATOMIC_ACQUIRE)) {}}
#define _ARC4_UNLOCK() { __atomic_clear(&gLock, __ATOMIC_RELEASE); }

static inline int _rs_allocate(struct _rs **, struct _rsx **);
#include "arc4random.h"

static inline void _rs_rekey(unsigned char *dat, size_t datlen);

static inline void
_rs_init(unsigned char *buf, size_t n) {
    if (n < KEYSZ + IVSZ)
        return;

    // allocate internal state if needed
    if (rs == NULL) {
        if (_rs_allocate(&rs, &rsx) == -1) {
            _exit(1);
        }
    }

    chacha_keysetup(&rsx->rs_chacha, buf, KEYSZ * 8, 0);
    chacha_ivsetup(&rsx->rs_chacha, buf + KEYSZ);
}

static void
_rs_stir(void)
{
    unsigned char rnd[KEYSZ + IVSZ];

    // acquire entropy
    int err = GetEntropy(rnd, sizeof rnd);
    if(err != sizeof rnd) {
        _getentropy_fail(err);
    }

    // initialize random context
    if (!rs) {
        _rs_init(rnd,  sizeof rnd);
    } else {
        _rs_rekey(rnd, sizeof rnd);
    }

    // discard source seed
    memset(rnd, 0, sizeof rnd);

    /* invalidate rs_buf */
    rs->rs_have = 0;
    memset(rsx->rs_buf, 0, sizeof rsx->rs_buf);

    rs->rs_count = 1600000;
}

static inline void
_rs_stir_if_needed(size_t len)
{
    if (!rs || rs->rs_count <= len)
        _rs_stir();
    if (rs->rs_count <= len)
        rs->rs_count = 0;
    else
        rs->rs_count -= len;
}

static inline void
_rs_rekey(unsigned char *dat, size_t datlen)
{
#ifndef KEYSTREAM_ONLY
    memset(rsx->rs_buf, 0, sizeof(rsx->rs_buf));
#endif
    /* fill rs_buf with the keystream */
    chacha_encrypt_bytes(&rsx->rs_chacha, rsx->rs_buf,
        rsx->rs_buf, sizeof(rsx->rs_buf));
    /* mix in optional user provided data */
    if (dat) {
        size_t i, m;

        m = minimum(datlen, KEYSZ + IVSZ);
        for (i = 0; i < m; i++)
                rsx->rs_buf[i] ^= dat[i];
    }
    /* immediately reinit for backtracking resistance */
    _rs_init(rsx->rs_buf, KEYSZ + IVSZ);
    memset(rsx->rs_buf, 0, KEYSZ + IVSZ);
    rs->rs_have = sizeof(rsx->rs_buf) - KEYSZ - IVSZ;
}

static inline void
_rs_random_buf(void *_buf, size_t n)
{
    unsigned char *buf = (unsigned char *)_buf;
    unsigned char *keystream;
    size_t m;

    _rs_stir_if_needed(n);
    while (n > 0) {
        if (rs->rs_have > 0) {
            m = minimum(n, rs->rs_have);
            keystream = rsx->rs_buf + sizeof(rsx->rs_buf)
                - rs->rs_have;
            memcpy(buf, keystream, m);
            memset(keystream, 0, m);
            buf += m;
            n -= m;
            rs->rs_have -= m;
        }
        if (rs->rs_have == 0)
            _rs_rekey(NULL, 0);
    }
}

static inline void
_rs_random_u32(uint32_t *val)
{
    unsigned char *keystream;

    _rs_stir_if_needed(sizeof(*val));
    if (rs->rs_have < sizeof(*val))
        _rs_rekey(NULL, 0);

    keystream = rsx->rs_buf + sizeof(rsx->rs_buf) - rs->rs_have;
    memcpy(val, keystream, sizeof(*val));
    memset(keystream, 0, sizeof(*val));
    rs->rs_have -= sizeof(*val);
}

LIBC_EXPORT uint32_t
arc4random(void)
{
    uint32_t val;

    _ARC4_LOCK();
    _rs_random_u32(&val);
    _ARC4_UNLOCK();
    return val;
}

LIBC_EXPORT void
arc4random_buf(void *buf, size_t n)
{
    _ARC4_LOCK();
    _rs_random_buf(buf, n);
    _ARC4_UNLOCK();
}
