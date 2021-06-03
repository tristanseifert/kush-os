#include <log.h>
#include <arch.h>
#include <platform.h>
#include <printf.h>

#include <arch/critical.h>
#include <arch/spinlock.h>
#include <arch/PerCpuInfo.h>

#include "debug/FramebufferConsole.h"
#include "sched/Scheduler.h"
#include "sched/Task.h"
#include "sched/Thread.h"

/// Framebuffer console which we can use to print to
namespace platform {
    extern debug::FramebufferConsole *gConsole;
};

/// Panic lock; this ensures we can only ever have one CPU core in the panic code at once
DECLARE_SPINLOCK_S(gPanicLock);

/**
 * printf wrapper to send a character to the debug spew port
 */
static void _outchar(char ch, void *ctx) {
    platform_debug_spew(ch);
}

/**
 * printf wrapper to send a character to the debug spew port and framebuffer console
 */
static void _outchar_panic(char ch, void *ctx) {
    using namespace platform;
    platform_debug_spew(ch);

    if(gConsole) {
        gConsole->write(ch);
    }
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
    auto sched = sched::Scheduler::get();
    auto thread = sched ? sched->runningThread() : nullptr;
    auto task = (thread ? thread->task : nullptr);

    // set up panic buffer
    constexpr static const size_t kPanicBufSz = 2048;
    static char panicBuf[kPanicBufSz];

    // format the message into it
    va_list va;
    va_start(va, format);

    vsnprintf(panicBuf, kPanicBufSz, format, va);

    va_end(va);

    fctprintf(_outchar_panic, 0, "\033[41m\033[;Hpanic: %s\npc: $%p\n", panicBuf, pc);

    if(thread) {
        fctprintf(_outchar_panic, 0, "  Active thread: %p (tid %u) '%s'\n",
                static_cast<void *>(thread), thread->tid, thread->name);
    }
    if(task) {
        fctprintf(_outchar_panic, 0, "    Active task: %p (pid %u) '%s'\n", static_cast<void *>(task),
                task->pid, task->name);
    }
    auto procLocal = arch::GetProcLocal();
    if(procLocal) {
        fctprintf(_outchar_panic, 0, "   Current core: %x\n", procLocal->procId);

    }

    fctprintf(_outchar_panic, 0, "Time since boot: %llu ns\n\n", platform_timer_now());

    // try to get a backtrace as well
    int err = arch_backtrace(nullptr, panicBuf, kPanicBufSz);
    if(err) {
        fctprintf(_outchar_panic, 0, "Backtrace:\n%s", panicBuf);
    }

    // reset terminal
    fctprintf(_outchar_panic, 0, "\033[m");

    // then jump to the platform panic handler
    platform_panic_handler();
}
