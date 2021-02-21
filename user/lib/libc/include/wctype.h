/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c)1999 Citrus Project,
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	citrus Id: wctype.h,v 1.4 2000/12/21 01:50:21 itojun Exp
 *	$NetBSD: wctype.h,v 1.3 2000/12/22 14:16:16 itojun Exp $
 * $FreeBSD$
 */

#ifndef _WCTYPE_H_
#define	_WCTYPE_H_

#ifndef _WCTRANS_T
typedef    int    wctrans_t;
#define    _WCTRANS_T
#endif

#ifndef _WCTYPE_T
typedef    unsigned long    wctype_t;
#define    _WCTYPE_T
#endif

#include <ctype.h>
#include <wchar.h>

int
iswalnum(wint_t wc);

int
iswalpha(wint_t wc);

int
iswascii(wint_t wc);

int
iswblank(wint_t wc);

int
iswcntrl(wint_t wc);

int
iswdigit(wint_t wc);

int
iswgraph(wint_t wc);

int
iswhexnumber(wint_t wc);

int
iswideogram(wint_t wc);

int
iswlower(wint_t wc);

int
iswnumber(wint_t wc);

int
iswphonogram(wint_t wc);

int
iswprint(wint_t wc);

int
iswpunct(wint_t wc);

int
iswrune(wint_t wc);

int
iswspace(wint_t wc);

int
iswspecial(wint_t wc);

int
iswupper(wint_t wc);

int
iswxdigit(wint_t wc);

int	iswalnum(wint_t);
int	iswalpha(wint_t);
int	iswblank(wint_t);
int	iswcntrl(wint_t);
int	iswctype(wint_t, wctype_t);
int	iswdigit(wint_t);
int	iswgraph(wint_t);
int	iswlower(wint_t);
int	iswprint(wint_t);
int	iswpunct(wint_t);
int	iswspace(wint_t);
int	iswupper(wint_t);
int	iswxdigit(wint_t);
wint_t	towctrans(wint_t, wctrans_t);
wint_t	towlower(wint_t);
wint_t	towupper(wint_t);
wctrans_t
	wctrans(const char *);
wctype_t
	wctype(const char *);


#endif		/* _WCTYPE_H_ */
