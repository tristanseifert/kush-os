/*	$NetBSD: ieeefp.h,v 1.4 1998/01/09 08:03:43 perry Exp $	*/
/* $FreeBSD$ */

/* 
 * Written by J.T. Conklin, Apr 6, 1995
 * Public domain.
 */

#ifndef _IEEEFP_H_
#define _IEEEFP_H_

#include <stdint.h>
#include <sys/cdefs.h>

#if defined(__i386__)
#include <ieeefp_x86.h>
#else
#error Add support to ieeefp.h for this architecture
#endif

#endif /* _IEEEFP_H_ */
