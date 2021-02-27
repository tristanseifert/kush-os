#include "Thread.h"

#include <platform.h>
#include <arch.h>
#include <log.h>
#include <string.h>

#include <vm/Mapper.h>
#include <vm/Map.h>
#include <sched/Scheduler.h>
#include <sched/Task.h>
#include <sched/Thread.h>

#include "syscall/Handler.h"

#include "exceptions.h"
#include "gdt.h"

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
    if(!thread->kernelMode || true) {
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
    bool yes = true, no = false;

    /*
     * Disable interrupts. When we return to the thread, we'll re-enable them, but this prevents
     * the stack from getting munged with from interrupts during a context switch.
     *
     * At the same time, we'll lower the irql back down to passive so that interrupts can get
     * queued again; but since they're disabled, we won't receive them until after the context
     * switch has completed.
     */
    asm volatile("cli" ::: "memory");
    platform_lower_irql(platform::Irql::Passive, false);

    REQUIRE(from != to, "cannot context switch same thread");

    // switch page tables if needed
    if((!from && to->task) ||
       (from && from->task && to->task && from->task != to->task)) {
        auto destVm = to->task->vm;

        /*
         * Skips activating the page table set if we're going to be switching to the kernel task
         * page table. Since kernel mappings are always shared, this saves us a pretty expensive
         * TLB flush the MOV to CR3 operation causes.
         */
        //if(::vm::Mapper::getKernelMap() != destVm) {
            destVm->activate();
        //}
    }

    // update TSS and thread-local pointer
    auto task = to->task;
    if(task) {
        const auto &taskState = task->archState;

        // the task has a custom TSS which needs activating
        if(taskState.hasTss) {
            tss_activate(taskState.tssIdx, reinterpret_cast<uintptr_t>(to->stack));
        }
        // otherwise, use system TSS
        else {
            // for kernel mode threads, the TSS should hold the per-CPU interrupt thread
            if(to->kernelMode) {
                // set the per-CPU kernel interrupt thread
                tss_set_esp0(nullptr);
            }
            // otherwise, store the thread's specially allocated kernel thread
            else {
                tss_set_esp0(to->stack);
            }
        }
    }

    gdt_update_tls_user(to->regs.gsBase);

    // update syscall handler state
    syscall::Handler::handleCtxSwitch(to);

    // save state into current thread and switch to next
    if(from) {
        //log("old task %%esp = %p, new task %%esp = %p (stack %p)", from->regs.stackTop, to->regs.stackTop, to->stack);
        // set the running flags
        __atomic_store(&from->isActive, &no, __ATOMIC_RELEASE);
        __atomic_store(&to->isActive, &yes, __ATOMIC_RELEASE);

        x86_switchto_save(&from->regs, &to->regs);
    }
    // no thread context to save; just switch
    else {
        // log("new task %%esp = %p (stack %p)", to->regs.stackTop, to->stack);
        __atomic_store(&to->isActive, &yes, __ATOMIC_RELEASE);

        x86_switchto(&to->regs);
    }
}

/**
 * Builds up a stack frame for use with IRET to return to ring 3.
 *
 * We make sure that on entry to the function, %edi contains the argument.
 */
void arch::ReturnToUser(const uintptr_t pc, const uintptr_t _stack, const uintptr_t arg) {
    // validate ranges
    REQUIRE(pc < 0xC0000000, "invalid user pc: %08x", pc);
    REQUIRE(_stack < 0xC0000000, "invalid user stack: %08x", pc);

    // switchyboi time
    x86_ring3_return(pc, _stack, arg);
}

/**
 * Pushes a stack frame to the top of the thread's stack that will cause a context switch to return
 * to the DPC handler routine, rather than the previous thread state.
 *
 * On return from the DPC handler, we perform another context switch to the real state of the
 * thread.
 *
 * We require that the thread cannot be scheduled during this time, and may not be running.
 */
int arch::PushDpcHandlerFrame(sched::Thread *thread) {

    // allocate stack space and get the previous restore frame if there is one
    const auto frameSz = sizeof(CpuRegs);

    CpuRegs *oldFrame = nullptr;
    auto frame = reinterpret_cast<CpuRegs *>((uintptr_t) thread->regs.stackTop - frameSz);

    if((uintptr_t) thread->stack < ((uintptr_t) thread->regs.stackTop + frameSz)) {
        oldFrame = reinterpret_cast<CpuRegs *>((uintptr_t) thread->regs.stackTop);
    }

    memset(frame, 0, frameSz);
    thread->regs.stackTop = frame;

    // fill it
    frame->eip = (uintptr_t) x86_dpc_stub;
    frame->ds = frame->es = frame->fs = frame->gs = GDT_KERN_DATA_SEG;

    // copy some data from the previous stack frame (if there is one)
    if(oldFrame) {
        frame->ebp = oldFrame->ebp;
        frame->eflags = oldFrame->eflags;

        log("previous frame %p eip %08x", oldFrame, oldFrame->eip);
    }

    // clean up
    return 0;
}

/**
 * Invokes DPCs on the current thread.
 */
extern "C" void x86_dpc_handler() {
    const auto irql = platform_raise_irql(platform::Irql::Dpc);

    auto thread = sched::Scheduler::get()->runningThread();
    thread->runDpcs();

    platform_lower_irql(irql);
}


/**
 * This is the function where threads that returned from their main function will end up.
 *
 * For now, this is a panic; but this probably should just delete the thread and move on.
 */
void x86_thread_end() {
    panic("thread returned from main");
}



/**
 * Releases the allocated TSS, if any, when a thread is deallocated.
 */
TaskState::~TaskState() {
    // release the TSS and its GDT entry
    if(this->hasTss) {
        this->hasTss = false;
        tss_release(this->tssIdx);
    }
}
