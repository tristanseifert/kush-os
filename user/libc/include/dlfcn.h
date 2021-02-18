/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _DLFCN_H_
#define	_DLFCN_H_

#include <stdint.h>

/*
 * Modes and flags for dlopen().
 */
#define	RTLD_LAZY	1	/* Bind function calls lazily. */
#define	RTLD_NOW	2	/* Bind function calls immediately. */
#define	RTLD_MODEMASK	0x3
#define	RTLD_GLOBAL	0x100	/* Make symbols globally available. */
#define	RTLD_LOCAL	0	/* Opposite of RTLD_GLOBAL, and the default. */
#define	RTLD_TRACE	0x200	/* Trace loaded objects and exit. */


/*
 * Request arguments for dlinfo().
 */
#define	RTLD_DI_LINKMAP		2	/* Obtain link map. */
#define	RTLD_DI_SERINFO		4	/* Obtain search path info. */
#define	RTLD_DI_SERINFOSIZE	5	/*  ... query for required space. */
#define	RTLD_DI_ORIGIN		6	/* Obtain object origin */
#define	RTLD_DI_MAX		RTLD_DI_ORIGIN

/*
 * Special handle arguments for dlsym()/dlinfo().
 */
#define	RTLD_NEXT	((void *) -1)	/* Search subsequent objects. */
#define	RTLD_DEFAULT	((void *) -2)	/* Use default search algorithm. */
#define	RTLD_SELF	((void *) -3)	/* Search the caller itself. */

typedef    struct dl_info {
    const char    *dli_fname;    /* Pathname of shared object. */
    void        *dli_fbase;    /* Base address of shared object. */
    const char    *dli_sname;    /* Name of nearest symbol. */
    void        *dli_saddr;    /* Address of nearest symbol. */
} Dl_info;

#ifdef __cplusplus
extern "C" {
#endif

int	 dlclose(void *);
char	*dlerror(void);
void	*dlopen(const char *, int);
void	*dlsym(void * __restrict, const char * __restrict);

int     dladdr(const void * __restrict, Dl_info * __restrict);
int     dlinfo(void * __restrict, int, void * __restrict);

#ifdef __cplusplus
}
#endif

#endif /* !_DLFCN_H_ */
