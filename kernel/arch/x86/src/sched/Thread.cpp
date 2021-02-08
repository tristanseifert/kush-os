#include "Thread.h"

#include <arch.h>
#include <log.h>
#include <string.h>

#include <vm/Map.h>
#include <sched/Task.h>
#include <sched/Thread.h>

#include "syscall/Handler.h"

#include "../exceptions.h"
#include "../gdt.h"

using namespace arch;


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
    /// XXX: the +8 allows us to build a stack frame to enter the main function
    const auto frameSz = sizeof(CpuRegs);
    uintptr_t *params = reinterpret_cast<uintptr_t *>((uintptr_t) thread->stack - 20);
    auto frame = reinterpret_cast<CpuRegs *>((uintptr_t) thread->stack - frameSz - 20);

    memset(frame, 0, frameSz);
    thread->regs.stackTop = frame;

    frame->eip = pc;
    frame->ds = frame->es = frame->fs = frame->gs = GDT_KERN_DATA_SEG;

    /**
     * Under the registers to restore, set up a stack frame for the thread to use when entering its
     * main method. This way, we can pass one (really, an arbitrary number, but we limit it to
     * just one) parameter to it as context.
     *
     * Note that we have some fudged offsets above to ensure the stack stays aligned.
     */
    params[0] = reinterpret_cast<uintptr_t>(x86_thread_end); // bogus return address
    params[1] = arg; // first argument

    // for threads ending up in userspace, ensure IRQs are on so they can be pre-empted
    if(!thread->kernelMode) {
        frame->eflags |= (1 << 9);
    }

    // EBP must be NULL for stack unwinding to work
    frame->ebp = 0;
}

/**
 * Restores the thread's state. We'll restore the FPU state, then execute the context switch by
 * switching to the correct stack, restoring registers and performing an iret.
 */
void arch::RestoreThreadState(sched::Thread *from, sched::Thread *to) {
    // switch page tables if needed
    if((!from && to->task) ||
       (from && from->task && to->task && from->task != to->task)) {
        to->task->vm->activate();
    }

    // for kernel mode threads, the TSS should hold the per-CPU interrupt thread
    if(to->kernelMode) {
        // set the per-CPU kernel interrupt thread
        tss_set_esp0(nullptr);
    }
    // otherwise, store the thread's specially allocated kernel thread
    else {
        tss_set_esp0(to->stack);
    }

    // update syscall handler state
    syscall::Handler::handleCtxSwitch(to);

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

/**
 * Builds up a stack frame for use with IRET to return to ring 3.
 */
void arch::ReturnToUser(const uintptr_t pc, const uintptr_t stack) {
    // validate ranges
    REQUIRE(pc < 0xC0000000, "invalid user pc: %08x", pc);
    REQUIRE(stack < 0xC0000000, "invalid user stack: %08x", pc);

    // switchyboi time
    x86_ring3_return(pc, stack);
}

/**
 * This is the function where threads that returned from their main function will end up.
 *
 * For now, this is a panic; but this probably should just delete the thread and move on.
 */
void x86_thread_end() {
    panic("thread returned from main");
}
