/*
 * Defines the format of the system information page that's mapped into every process.
 *
 * The format (and really, its existence) of this table should be considered an implementation
 * detail, and you should not rely on this structure in any way, except for system library calls
 * that vend information from it.
 */
#ifndef _LIBSYSTEM_SYS_INFOPAGE_H
#define _LIBSYSTEM_SYS_INFOPAGE_H

#include <stdint.h>

#define _KSIP_MAGIC                     'KUSH'
#define _KSIP_VERSION_CURRENT           1

/**
 * System information structure; this same data structure is mapped into every process at an
 * implementation defined address.
 */
typedef struct __system_info {
    /// magic value: must be 'KUSH'
    uint32_t magic;
    /// version of the structure, current is 1
    uint32_t version;

    /// system page size, in bytes
    uintptr_t pageSz;

    /// lookup service handle
    uintptr_t dispensaryPort;
} kush_sysinfo_page_t;

#endif
