#ifndef DYLDOSRV_PACKETTYPES_H
#define DYLDOSRV_PACKETTYPES_H

#include <sys/elf.h>

/**
 * Message types for the dynamic linker packet server
 */
enum class DyldosrvMessageType: uint32_t {
    /// Request mapping for a shared library
    MapSegment                          = 'SEGM',
    /// Reply for mapping a shared library's segment
    MapSegmentReply                     = 'SEGR',

    /// Indicates that the file IO connection should be reset
    RootFsUpdated                       = 'FSUP',
};

/**
 * Error codes for the Dyldosrv interface
 */
enum DyldosrvErrors: int {
    InternalError                       = -48400,
};

/**
 * Request to map a particular shared library's segment.
 *
 * This will look up if we've already loaded this segment, and if so, simply maps it into the
 * address space of the caller at the requested address. If we haven't, we'll load it and perform
 * the same task.
 *
 * Note: the task into which the object is mapped is identified by the RPC message.
 */
struct DyldosrvMapSegmentRequest {
    /// Virtual base address of this shared object
    uintptr_t objectVmBase;
    /// ELF program header of the segment requested
    Elf_Phdr phdr;

    /// Zero terminated string containing the full path of the library
    char path[];
};

/**
 * Reply to a request to map a particular segment.
 */
struct DyldosrvMapSegmentReply {
    /// Status code: 0 indicates success
    int32_t status;

    /// VM handle of the region that was mapped in
    uintptr_t vmRegion;
};

#endif
