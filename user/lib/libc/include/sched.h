#ifndef LIBC_SCHED_H
#define LIBC_SCHED_H

#include <_libc.h>

/// Yields remaining CPU time
LIBC_EXPORT int sched_yield(void);

#endif
