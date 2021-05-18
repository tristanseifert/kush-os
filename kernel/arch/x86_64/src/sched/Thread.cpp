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
 * In our case, we simply stuff the argument into the %rdi register since that's where the first
 * argument in x86_64 goes.
 *
 * Kernel threads do not cause a protection mode switch; this means the ESP/SS words at the very
 * end of the stack frame aren't popped. Therefore, we just stuff the argument into where the
 * stack segment goes otherwise.
 */
void arch::InitThreadState(sched::Thread *thread, const uintptr_t pc, const uintptr_t arg) {
    // make space for the exception frame and initialize it
    const auto frameSz = sizeof(CpuRegs);
    uintptr_t *params = reinterpret_cast<uintptr_t *>((uintptr_t) thread->stack - 20);
    auto frame = reinterpret_cast<CpuRegs *>((uintptr_t) thread->stack - frameSz - 20);

    memset(frame, 0, frameSz);
    thread->regs.stackTop = frame;

    frame->rip = pc;

    // return argument
    frame->rdi = arg;
    // bogus return address
    params[0] = reinterpret_cast<uintptr_t>(amd64_thread_end);

    // for threads ending up in userspace, ensure IRQs are on so they can be pre-empted
    if(!thread->kernelMode || true) {
        frame->rflags |= (1 << 9);
    }

    // RBP must be NULL for stack unwinding to work
    frame->rbp = 0;
}

/**
 * Restores the thread's state. We'll restore the FPU state, then execute the context switch by
 * switching to the correct stack, restoring registers and performing an iret.
 */
void arch::RestoreThreadState(const rt::SharedPtr<sched::Thread> &from,
        const rt::SharedPtr<sched::Thread> &to) {
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

    // update thread-local address (%gs and %fs)
    x86_msr_write(X86_MSR_KERNEL_GSBASE, (to->regs.gsBase), (to->regs.gsBase) >> 32ULL);
    x86_msr_write(X86_MSR_FSBASE, (to->regs.fsBase), (to->regs.fsBase) >> 32ULL);

    // update syscall handler and kernel stack in TSS
    syscall::Handler::handleCtxSwitch(to);

    // TSS stack is kernel stack of thread (for scheduler IPI)
    const auto toStackAddr = reinterpret_cast<uintptr_t>(to->stack);
    auto tss = PerCpuInfo::get()->tss;
    REQUIRE(tss, "failed to get tss ptr");

    tss->rsp[0].low =  (toStackAddr & 0xFFFFFFFF);
    tss->rsp[0].high = (toStackAddr >> 32ULL);

    // save state into current thread and switch to next
    if(from) {
        // stop the task timer
        sched::Scheduler::get()->willSwitchFrom(from);

        //log("old task %%esp = %p, new task %%esp = %p (stack %p)", from->regs.stackTop, to->regs.stackTop, to->stack);
        // set the running flags
        __atomic_store(&from->isActive, &no, __ATOMIC_RELAXED);
        __atomic_store(&to->isActive, &yes, __ATOMIC_RELAXED);
        __atomic_thread_fence(__ATOMIC_ACQUIRE);

        sched::Scheduler::get()->willSwitchTo(to);
        amd64_switchto_save(&from->regs, &to->regs);
    }
    // no thread context to save; just switch
    else {
        // log("new task %%esp = %p (stack %p)", to->regs.stackTop, to->stack);
        __atomic_store(&to->isActive, &yes, __ATOMIC_RELAXED);
        __atomic_thread_fence(__ATOMIC_ACQUIRE);

        sched::Scheduler::get()->willSwitchTo(to);
        amd64_switchto(&to->regs);
    }
}

/**
 * Builds up a stack frame for use with IRET to return to ring 3.
 *
 * We make sure that on entry to the function, %edi contains the argument.
 */
void arch::ReturnToUser(const uintptr_t pc, const uintptr_t _stack, const uintptr_t arg) {
    // validate ranges
    REQUIRE(pc < 0x8000000000000000, "invalid user pc: %016llx", pc);
    REQUIRE(_stack < 0x8000000000000000, "invalid user stack: %016llx", pc);

    // switchyboi time
    amd64_ring3_return(pc, _stack, arg);
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
    frame->rip = (uintptr_t) amd64_dpc_stub;

    // copy some data from the previous stack frame (if there is one)
    if(oldFrame) {
        frame->rbp = oldFrame->rbp;
        frame->rflags = oldFrame->rflags;

        log("previous frame %p rip %016lx", oldFrame, oldFrame->rip);
    }

    // clean up
    return 0;
}

/**
 * Invokes DPCs on the current thread.
 */
extern "C" void amd64_dpc_handler() {
    const auto irql = platform_raise_irql(platform::Irql::Dpc);

    auto thread = sched::Thread::current();
    thread->runDpcs();

    platform_lower_irql(irql);
}


/**
 * This is the function where threads that returned from their main function will end up.
 *
 * For now, this is a panic; but this probably should just delete the thread and move on.
 */
void amd64_thread_end() {
    panic("thread returned from main");
}

