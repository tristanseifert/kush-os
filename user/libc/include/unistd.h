/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1991, 1993, 1994
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
 *	@(#)unistd.h	8.12 (Berkeley) 4/27/95
 * $FreeBSD$
 */

#ifndef _UNISTD_H_
#define	_UNISTD_H_

#include <stddef.h>
#include <stdint.h>

#ifndef _GID_T_DECLARED
typedef    uint16_t        gid_t;
#define    _GID_T_DECLARED
#endif

#ifndef _OFF_T_DECLARED
typedef    int        off_t;
#define    _OFF_T_DECLARED
#endif

#ifndef _SSIZE_T_DECLARED
typedef    long        ssize_t;
#define    _SSIZE_T_DECLARED
#endif

#ifndef _PID_T_DECLARED
typedef    uint32_t        pid_t;
#define    _PID_T_DECLARED
#endif

#ifndef _UID_T_DECLARED
typedef    uint16_t        uid_t;
#define    _UID_T_DECLARED
#endif

#ifndef _USECONDS_T_DECLARED
typedef    size_t    useconds_t;
#define    _USECONDS_T_DECLARED
#endif

#define	STDIN_FILENO	0	/* standard input file descriptor */
#define	STDOUT_FILENO	1	/* standard output file descriptor */
#define	STDERR_FILENO	2	/* standard error file descriptor */

/*
 * POSIX-style system configuration variable accessors (for the
 * sysconf function).  The kernel does not directly implement the
 * sysconf() interface; rather, a C library stub translates references
 * to sysconf() into calls to sysctl() using a giant switch statement.
 * Those that are marked `user' are implemented entirely in the C
 * library and never query the kernel.  pathconf() is implemented
 * directly by the kernel so those are not defined here.
 */
#define	_SC_ARG_MAX		 1
#define	_SC_CHILD_MAX		 2
#define	_SC_CLK_TCK		 3
#define	_SC_NGROUPS_MAX		 4
#define	_SC_OPEN_MAX		 5
#define	_SC_JOB_CONTROL		 6
#define	_SC_SAVED_IDS		 7
#define	_SC_VERSION		 8
#define	_SC_BC_BASE_MAX		 9 /* user */
#define	_SC_BC_DIM_MAX		10 /* user */
#define	_SC_BC_SCALE_MAX	11 /* user */
#define	_SC_BC_STRING_MAX	12 /* user */
#define	_SC_COLL_WEIGHTS_MAX	13 /* user */
#define	_SC_EXPR_NEST_MAX	14 /* user */
#define	_SC_LINE_MAX		15 /* user */
#define	_SC_RE_DUP_MAX		16 /* user */
#define	_SC_2_VERSION		17 /* user */
#define	_SC_2_C_BIND		18 /* user */
#define	_SC_2_C_DEV		19 /* user */
#define	_SC_2_CHAR_TERM		20 /* user */
#define	_SC_2_FORT_DEV		21 /* user */
#define	_SC_2_FORT_RUN		22 /* user */
#define	_SC_2_LOCALEDEF		23 /* user */
#define	_SC_2_SW_DEV		24 /* user */
#define	_SC_2_UPE		25 /* user */
#define	_SC_STREAM_MAX		26 /* user */
#define	_SC_TZNAME_MAX		27 /* user */

#if __POSIX_VISIBLE >= 199309
#define	_SC_ASYNCHRONOUS_IO	28
#define	_SC_MAPPED_FILES	29
#define	_SC_MEMLOCK		30
#define	_SC_MEMLOCK_RANGE	31
#define	_SC_MEMORY_PROTECTION	32
#define	_SC_MESSAGE_PASSING	33
#define	_SC_PRIORITIZED_IO	34
#define	_SC_PRIORITY_SCHEDULING	35
#define	_SC_REALTIME_SIGNALS	36
#define	_SC_SEMAPHORES		37
#define	_SC_FSYNC		38
#define	_SC_SHARED_MEMORY_OBJECTS 39
#define	_SC_SYNCHRONIZED_IO	40
#define	_SC_TIMERS		41
#define	_SC_AIO_LISTIO_MAX	42
#define	_SC_AIO_MAX		43
#define	_SC_AIO_PRIO_DELTA_MAX	44
#define	_SC_DELAYTIMER_MAX	45
#define	_SC_MQ_OPEN_MAX		46
#define	_SC_PAGESIZE		47
#define	_SC_RTSIG_MAX		48
#define	_SC_SEM_NSEMS_MAX	49
#define	_SC_SEM_VALUE_MAX	50
#define	_SC_SIGQUEUE_MAX	51
#define	_SC_TIMER_MAX		52
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* 1003.1-1990 */
void	 _exit(int);
int	 access(const char *, int);
unsigned int	 alarm(unsigned int);
int	 chdir(const char *);
int	 chown(const char *, uid_t, gid_t);
int	 close(int);
void	 closefrom(int);
int	 dup(int);
int	 dup2(int, int);
int	 execl(const char *, const char *, ...) __attribute__ ((sentinel));
int	 execle(const char *, const char *, ...);
int	 execlp(const char *, const char *, ...) __attribute__ ((sentinel));
int	 execv(const char *, char * const *);
int	 execve(const char *, char * const *, char * const *);
int	 execvp(const char *, char * const *);
pid_t	 fork(void);
long	 fpathconf(int, int);
char	*getcwd(char *, size_t);
gid_t	 getegid(void);
uid_t	 geteuid(void);
gid_t	 getgid(void);
int	 getgroups(int, gid_t []);
char	*getlogin(void);
pid_t	 getpgrp(void);
pid_t	 getpid(void);
pid_t	 getppid(void);
uid_t	 getuid(void);
int	 isatty(int);
int	 link(const char *, const char *);
#ifndef _LSEEK_DECLARED
#define	_LSEEK_DECLARED
off_t	 lseek(int, off_t, int);
#endif
long	 pathconf(const char *, int);
int	 pause(void);
int	 pipe(int *);
ssize_t	 read(int, void *, size_t);
int	 rmdir(const char *);
int	 setgid(gid_t);
int	 setpgid(pid_t, pid_t);
pid_t	 setsid(void);
int	 setuid(uid_t);
unsigned int	 sleep(unsigned int);
long	 sysconf(int);
pid_t	 tcgetpgrp(int);
int	 tcsetpgrp(int, pid_t);
char	*ttyname(int);
int	ttyname_r(int, char *, size_t);
int	 unlink(const char *);
ssize_t	 write(int, const void *, size_t);

/* 1003.2-1992 */
#if __POSIX_VISIBLE >= 199209 || __XSI_VISIBLE
size_t	 confstr(int, char *, size_t);
#ifndef _GETOPT_DECLARED
#define	_GETOPT_DECLARED
int	 getopt(int, char * const [], const char *);

extern char *optarg;			/* getopt(3) external variables */
extern int optind, opterr, optopt;
#endif /* _GETOPT_DECLARED */
#endif

/* ISO/IEC 9945-1: 1996 */
#if __POSIX_VISIBLE >= 199506 || __XSI_VISIBLE
int	 fsync(int);
int	 fdatasync(int);

/*
 * ftruncate() was in the POSIX Realtime Extension (it's used for shared
 * memory), but truncate() was not.
 */
#ifndef _FTRUNCATE_DECLARED
#define	_FTRUNCATE_DECLARED
int	 ftruncate(int, off_t);
#endif
#endif

#if __POSIX_VISIBLE >= 199506
int	 getlogin_r(char *, size_t);
#endif

/* 1003.1-2001 */
#if __POSIX_VISIBLE >= 200112 || __XSI_VISIBLE
int	 fchown(int, uid_t, gid_t);
ssize_t	 readlink(const char * __restrict, char * __restrict, size_t);
#endif
#if __POSIX_VISIBLE >= 200112
int	 gethostname(char *, size_t);
int	 setegid(gid_t);
int	 seteuid(uid_t);
#endif

#ifdef __cplusplus
}
#endif

#endif /* !_UNISTD_H_ */
