#include <log.h>
#include <arch.h>
#include <platform.h>
#include <printf.h>

#include <arch/critical.h>
#include <arch/spinlock.h>

#include "sched/Scheduler.h"
#include "sched/Task.h"
#include "sched/Thread.h"

/// Panic lock; this ensures we can only ever have one CPU core in the panic code at once
DECLARE_SPINLOCK_S(gPanicLock);

/**
 * printf wrapper to send a character to the debug spew port
 */
static void _outchar(char ch, void *ctx) {
    platform_debug_spew(ch);
}

/**
 * Writes the message to the kernel log.
 *
 * This will be an in-memory buffer, as well as optionally a debug spew port defined by the
 * platform code.
 */
void log(const char *format, ...) {
    va_list va;
    va_start(va, format);

    fctprintf(_outchar, NULL, "[%16llu] ", platform_timer_now());
    fctvprintf(_outchar, NULL, format, va);

    va_end(va);

    platform_debug_spew('\n');
}

/**
 * Take the panic lock, write the message, then halt the system.
 */
void panic(const char *format, ...) {
    void *pc = __builtin_return_address(0);

    // take panic lock
    SPIN_LOCK(gPanicLock);
    platform_raise_irql(platform::Irql::CriticalSection, false);

    // get current thread
    auto thread = sched::Scheduler::get()->runningThread();
    auto task = (thread ? thread->task : nullptr);

    // set up panic buffer
    static const size_t kPanicBufSz = 2048;
    static char panicBuf[2048];

    // format the message into it
    va_list va;
    va_start(va, format);

    vsnprintf(panicBuf, kPanicBufSz, format, va);

    va_end(va);

    fctprintf(_outchar, NULL, "panic: %s\npc: $%p\n", panicBuf, pc);

    if(thread) {
        fctprintf(_outchar, NULL, "  Active thread: %p (tid %u) '%s'\n", thread, thread->tid,
                thread->name);
    }
    if(task) {
        fctprintf(_outchar, NULL, "    Active task: %p (pid %u) '%s'\n", task, task->pid,
                task->name);
    }

    fctprintf(_outchar, NULL, "Time since boot: %llu ns\n", platform_timer_now());

    // try to get a backtrace as well
    int err = arch_backtrace(NULL, panicBuf, kPanicBufSz);
    if(err) {
        fctprintf(_outchar, NULL, "Backtrace:\n%s", panicBuf);
    }

    // then jump to the platform panic handler
    platform_panic_handler();
}
