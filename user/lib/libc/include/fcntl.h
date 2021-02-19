#ifndef _LIBC_FCNTL_H
#define _LIBC_FCNTL_H

#include <stdint.h>

#ifndef _MODE_T_DECLARED
typedef    uintptr_t    mode_t;
#define    _MODE_T_DECLARED
#endif

/* open-only flags */
#define    O_RDONLY    0x0000        /* open for reading only */
#define    O_WRONLY    0x0001        /* open for writing only */
#define    O_RDWR        0x0002        /* open for reading and writing */
#define    O_ACCMODE    0x0003        /* mask for above modes */
#define    O_NONBLOCK    0x0004        /* no delay */
#define    O_APPEND    0x0008        /* set append mode */
#define    O_SYNC        0x0080        /* POSIX synonym for O_FSYNC */
#define    O_CREAT        0x0200        /* create if nonexistent */
#define    O_TRUNC        0x0400        /* truncate to zero length */
#define    O_EXCL        0x0800        /* error if already exists */

#ifdef __cplusplus
extern "C" {
#endif

int    open(const char *, int, ...);
int    creat(const char *, mode_t);
int    fcntl(int, int, ...);

#ifdef __cplusplus
}
#endif


#endif
