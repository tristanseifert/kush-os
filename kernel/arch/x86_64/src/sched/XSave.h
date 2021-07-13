/**
 * Implements support for the XSAVE processor feature to maintain the legacy floating point and new
 * streaming vector extension state.
 */
#ifndef ARCH_x86_64_SCHED_XSAVE_H
#define ARCH_x86_64_SCHED_XSAVE_H

#include <stddef.h>

namespace arch {
struct ThreadState;

/// Initialize the XSAVE feature set.
void InitXSave();
/// Initializes the thread's XSAVE region.
void AllocXSaveRegion(ThreadState &ts);

/// Get size of the region required for XSAVE state
size_t GetXSaveRegionSize();
}

#endif
