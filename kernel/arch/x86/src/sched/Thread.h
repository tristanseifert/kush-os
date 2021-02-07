#ifndef ARCH_X86_THREAD_H
#define ARCH_X86_THREAD_H

#include "ThreadState.h"

namespace sched {
struct Thread;
}

namespace arch {
/**
 * Initializes a thread's state so that it will begin executing at the given address.
 */
void InitThreadState(sched::Thread *thread, const uintptr_t pc, const uintptr_t arg);

/**
 * Restores thread state.
 *
 * If the previous thread pointer is non-null, we'll save the current processor state into it. That
 * means that when that thread is switched back in, it'll be as if it were returning from this
 * function; inevitably, this would probably lead in either a syscall return, or an interrupt frame
 * being popped, returning to that task.
 */
void RestoreThreadState(sched::Thread *from, sched::Thread *to);
}

#endif
