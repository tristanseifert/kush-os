/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)time.h	8.3 (Berkeley) 1/21/94
 */
#include <stdint.h>
#include <stddef.h>

#ifndef _TIME_H_
#define	_TIME_H_

/* Frequency of the clock ticks reported by clock().  */
#define	CLOCKS_PER_SEC	1000

/*
 * Supported clock types
 */
#define CLOCK_REALTIME          0
#define CLOCK_MONOTONIC         4

#ifndef _CLOCK_T_DECLARED
typedef	uint64_t clock_t;
#define	_CLOCK_T_DECLARED
#endif
#ifndef _CLOCKID_T_DECLARED
typedef unsigned int clockid_t;
#define    _CLOCKID_T_DECLARED
#endif

#ifndef _TIME_T_DECLARED
typedef	unsigned int time_t;
#define	_TIME_T_DECLARED
#endif

struct tm {
	int	tm_sec;		/* seconds after the minute [0-60] */
	int	tm_min;		/* minutes after the hour [0-59] */
	int	tm_hour;	/* hours since midnight [0-23] */
	int	tm_mday;	/* day of the month [1-31] */
	int	tm_mon;		/* months since January [0-11] */
	int	tm_year;	/* years since 1900 */
	int	tm_wday;	/* days since Sunday [0-6] */
	int	tm_yday;	/* days since January 1 [0-365] */
	int	tm_isdst;	/* Daylight Savings Time flag */
	long	tm_gmtoff;	/* offset from UTC in seconds */
	char	*tm_zone;	/* timezone abbreviation */
};

struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

#ifdef __cplusplus
extern "C" {
#endif

char *asctime(const struct tm *);
clock_t clock(void);
char *ctime(const time_t *);
double difftime(time_t, time_t);

/* XXX missing: getdate() */
struct tm *gmtime(const time_t *);
struct tm *localtime(const time_t *);
time_t mktime(struct tm *);
size_t strftime(char * __restrict, size_t, const char * __restrict,
    const struct tm * __restrict);
time_t time(time_t *);

int clock_getres(clockid_t, struct timespec *);
int clock_gettime(clockid_t, struct timespec *);
int clock_settime(clockid_t, const struct timespec *);
int nanosleep(const struct timespec *, struct timespec *);

char *asctime_r(const struct tm *, char *);
char *ctime_r(const time_t *, char *);
struct tm *gmtime_r(const time_t *, struct tm *);
struct tm *localtime_r(const time_t *, struct tm *);


#ifdef __cplusplus
}
#endif

#endif /* !_TIME_H_ */
