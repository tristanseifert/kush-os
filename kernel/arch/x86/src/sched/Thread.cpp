#include "Thread.h"

#include <log.h>
#include <string.h>
#include <sched/Thread.h>

#include "../exceptions.h"
#include "../gdt.h"

using namespace arch;

extern "C" void x86_switchto(ThreadState *to) __attribute__((noreturn));
extern "C" void x86_switchto_save(ThreadState *from, ThreadState *to);

/**
 * Initializes a thread's state. Execution will begin at the given address, passing the given
 * pointer-sized argument to the function.
 *
 * In our case, we'll simply push it to the stack of the thread as the C calling convention
 * indicates.
 *
 * Kernel threads do not cause a protection mode switch; this means the ESP/SS words at the very
 * end of the stack frame aren't popped. Therefore, we just stuff the argument into where the
 * stack segment goes otherwise.
 */
void arch::InitThreadState(sched::Thread *thread, const uintptr_t pc, const uintptr_t arg) {
    // make space for the exception frame and initialize it
    const auto frameSz = sizeof(x86_exception_info_t);
    auto frame = reinterpret_cast<x86_exception_info_t *>((uintptr_t) thread->stack - frameSz - sizeof(arg));

    memset(frame, 0, frameSz);
    thread->regs.stackTop = frame;

    frame->eip = pc;

    if(thread->kernelMode) {
        frame->cs = GDT_KERN_CODE_SEG;
        frame->ds = frame->es = frame->fs = frame->gs = frame->ss = GDT_KERN_DATA_SEG;

        /**
         * When we return to the thread code, we do so using IRET; it doesn't read the last two
         * words off the stack frame (stack segment and stack ptr) if the privilege level doesn't
         * change. To avoid munging around with the structs, we just place the parameter into the
         * stack segment field, since that's where the function invoked as the thread main will
         * read it from.
         *
         * This is a little hacky, and depends entirely on how the x86 C ABI works, but it should
         * at least be stable.
         *
         * Potentially problematic is the resulting stack being 8 byte aligned rather than 16, but
         * we don't use any SIMD code in the kernel so it should ok?
         */
        frame->ss = arg;
    } else {
        frame->cs = GDT_USER_CODE_SEG | 3;
        frame->ds = frame->es = frame->fs = frame->gs = frame->ss = GDT_USER_DATA_SEG | 3;

        // ensure IRQs will be enabled
        frame->eflags |= (1 << 9);

        // TODO: set up usermode stack?
    }

    // EBP must be NULL for stack unwinding to work
    frame->ebp = 0;
}

/**
 * Restores the thread's state. We'll restore the FPU state, then execute the context switch by
 * switching to the correct stack, restoring registers and performing an iret.
 */
void arch::RestoreThreadState(sched::Thread *from, sched::Thread *to) {
    // for kernel mode threads, the TSS should hold the per-CPU interrupt thread
    if(to->kernelMode) {
        // set the per-CPU kernel interrupt thread
        tss_set_esp0(nullptr);
    }
    // otherwise, store the thread's specially allocated kernel thread
    else {
        tss_set_esp0(to->stack);
    }

    // save state into current thread and switch to next
    if(from) {
        // log("old task %%esp = %p, new task %%esp = %p (stack %p)", from->regs.stackTop, to->regs.stackTop, to->stack);
        x86_switchto_save(&from->regs, &to->regs);
    }
    // no thread context to save; just switch
    else {
        // log("new task %%esp = %p (stack %p)", to->regs.stackTop, to->stack);
        x86_switchto(&to->regs);
    }
}
