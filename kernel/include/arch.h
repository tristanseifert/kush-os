#ifndef KERNEL_ARCH_H
#define KERNEL_ARCH_H

#ifndef __cplusplus
#include <stdbool.h>
#endif

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initializes the architecture. Called early during boot.
 */
void arch_init();

/**
 * Notifies the architecture code that paging and virtual memory has become available.
 */
void arch_vm_available();

/**
 * Returns the size of a page.
 */
size_t arch_page_size();

/**
 * Whether the processor supports marking regions as no-execute.
 */
bool arch_supports_nx();



/**
 * Outputs a backtrace to the given string buffer.
 */
int arch_backtrace(void *stack, char *buf, const size_t bufLen);

#ifdef __cplusplus
}

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

/**
 * Performs a return to user mode code, with the given instruction pointer and stack pointer.
 *
 * This call does not return; it's usually only used to initialize an userspace thread, since after
 * that, the only way it'll get to the kernel is a syscall or interrupt, and we return back to it
 * using the standard syscall exit/interrupt return mechanism.
 *
 * Stack pointer should point to the base (bottom) of the stack. We'll build up an architecture-
 * specific stack frame that's interpreted by the runtime entry point (which is to say, some
 * userspace library, either a statically linked part of the binary or the dynamic linker) before
 * it goes and jumps to the "real" program entry point.
 *
 * Implementations of this function should assert that both the stack and program counter values
 * do not fall into kernel regions.
 */
void ReturnToUser(const uintptr_t pc, const uintptr_t stack) __attribute__((noreturn));
}
#endif
#endif
