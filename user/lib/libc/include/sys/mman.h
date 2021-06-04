#ifndef LIBC_SYS_MMAN_H
#define LIBC_SYS_MMAN_H

#include <_libc.h>
#include <sys/types.h>

/*
 * Protections are chosen from these bits, or-ed together
 */
/// pages cannot be accessed
#define PROT_NONE                       0x00
/// pages are readable
#define PROT_READ                       0x01
/// pages are writeable
#define PROT_WRITE                      0x02
/// pages are executable
#define PROT_EXEC                       0x04

/*
 * Flags contain sharing type and options.
 * Sharing types; choose one.
 */
/// Changes made to mapping are propagated to other views
#define MAP_SHARED                      0x0001
/// Changes do not propagate to other views
#define MAP_PRIVATE                     0x0002

/*
 * Other flags
 */
/// The object must be mapped at the provided address or fail
#define MAP_FIXED                       0x0010
/// The memory object is backed by anonymous memory
#define MAP_ANON                        0x1000
// for POSIX compatibility
#define MAP_ANONYMOUS                   MAP_ANON
// mask for flag values
#define MAP_FLAGMASK                    0xfff7

/*
 * Flags for msync
 */
/// Perform asynchronous writes to disk
#define MS_ASYNC                        0x01
/// Invalidate mappings
#define MS_INVALIDATE                   0x02
/// Perform synchronous writes to disk
#define MS_SYNC                         0x04

/*
 * Flags for mlockall
 */
/// Lock all pages currently mapped
#define MCL_CURRENT                     0x01
/// Lock all pages allocated in the future
#define MCL_FUTURE                      0x02

LIBC_EXPORT int    mlock(const void *, size_t);
LIBC_EXPORT int    mlockall(int);
LIBC_EXPORT void  *mmap(void *, size_t, int, int, int, off_t);
LIBC_EXPORT int    mprotect(void *, size_t, int);
LIBC_EXPORT int    msync(void *, size_t, int);
LIBC_EXPORT int    munlock(const void *, size_t);
LIBC_EXPORT int    munlockall(void);
LIBC_EXPORT int    munmap(void *, size_t);

#endif
