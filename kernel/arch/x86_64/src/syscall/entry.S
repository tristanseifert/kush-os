#define ASM_FILE
#include "Handler.h"
#include "PerCpuInfo.h"

/**
 * Save all registers that we may clobber during the syscall.
 *
 * Note that the registers RDI, RSI, RDX, R10, R8, R9 are missing, because they are used to pass
 * parameters, so we can save time by not restoring them. The same goes for RAX, which holds the
 * system call number. (R9 is saved anyways though because it's a temporary for stack ptr)
 *
 * A total of 8 quadwords are pushed, ensuring 16-byte alignment of the resulting stack as required
 * by the ABI.
 */
.macro state_save
    pushq        %rbx
    pushq        %rcx
    pushq        %rbp
    pushq        %r9
    pushq        %r11
    pushq        %r12
    pushq        %r13
    pushq        %r14
    pushq        %r15
.endm
/// Macro to restore syscall state, companion to state_save
.macro state_restore
    popq         %r15
    popq         %r14
    popq         %r13
    popq         %r12
    popq         %r11
    popq         %r9
    popq         %rbp
    popq         %rcx
    popq         %rbx
.endm

/**
 * System call table; this is used by the syscall entry code to dispatch to the correct handler
 * routine. It's important that this is kept up-to-date for ALL other architectures as well.
 *
 * It's required to use the mangled C++ names of functions here.
 */
.align 0x40
arch_syscall_table:
    // 0x00: Receive message
    .quad       _ZN3sys11PortReceiveE6HandlePNS_8RecvInfoEmm
    // 0x01: Send message
    .quad       _ZN3sys8PortSendE6HandlePKvm
    // 0x02: Update port parameters
    .quad       _ZN3sys13PortSetParamsE6Handlem
    /// 0x03: Allocate port
    .quad       _ZN3sys9PortAllocEv
    // 0x04: Deallocate port
    .quad       _ZN3sys11PortDeallocE6Handle
    // 0x05: Share virtual memory region
    .quad       _ZN3sys7Syscall20UnimplementedSyscallEv
    // 0x06-0x07: reserved
    .quad       _ZN3sys7Syscall20UnimplementedSyscallEv
    .quad       _ZN3sys7Syscall20UnimplementedSyscallEv

    // 0x08: Receive notification
    .quad       _ZN3sys13NotifyReceiveEmm
    // 0x09: Send notification
    .quad       _ZN3sys10NotifySendE6Handlem
    // 0x0A-0x0F: Reserved
    .rept       6
    .quad       _ZN3sys7Syscall20UnimplementedSyscallEv
    .endr

    // 0x10: Create VM region
    .quad       _ZN3sys17VmAllocPhysRegionEmmNS_7VmFlagsE
    // 0x11: Create anonymous VM region
    .quad       _ZN3sys17VmAllocAnonRegionEmNS_7VmFlagsE
    // 0x12: Deallocate VM region
    .quad       _ZN3sys9VmDeallocE6Handle
    // 0x13: Update VM region permissions
    .quad       _ZN3sys25VmRegionUpdatePermissionsE6HandleNS_7VmFlagsE
    // 0x14: Resize VM region
    .quad       _ZN3sys14VmRegionResizeE6HandlemNS_7VmFlagsE
    // 0x15: Map VM region (basic)
    .quad       _ZN3sys11VmRegionMapE6HandleS0_mmNS_7VmFlagsE
    // 0x16: Map VM region (advanced)
    .quad       _ZN3sys13VmRegionMapExE6HandleS0_PNS_12VmMapRequestEm
    // 0x17: Unmaps VM region
    .quad       _ZN3sys13VmRegionUnmapE6HandleS0_
    // 0x18: Get VM region info
    .quad       _ZN3sys15VmRegionGetInfoE6HandleS0_PNS_6VmInfoEm
    // 0x19: Get task VM info
    .quad       _ZN3sys13VmTaskGetInfoE6HandlePNS_10VmTaskInfoEm
    // 0x1A: Convert virtual address to region handle
    .quad       _ZN3sys14VmAddrToRegionE6Handlem
    // 0x1B: Convert virtual addresses to physical
    .quad       _ZN3sys21VmTranslateVirtToPhysE6HandlePKmPmm
    // 0x1C: Query memory subsystem information
    .quad       _ZN3sys13VmQueryParamsENS_10VmQueryKeyEPvm

    // 0x1D-0x1F: VM calls (reserved)
    .rept       3
    .quad       _ZN3sys7Syscall20UnimplementedSyscallEv
    .endr

    // 0x20: Return current thread handle
    .quad       _ZN3sys15ThreadGetHandleEv
    // 0x21: Give up remaining CPU quantum
    .quad       _ZN3sys11ThreadYieldEv
    // 0x22: Microsecond granularity sleep
    .quad       _ZN3sys12ThreadUsleepEm
    // 0x23: Create thread
    .quad       _ZN3sys12ThreadCreateEmmmm
    // 0x24: Join with thread
    .quad       _ZN3sys10ThreadJoinE6Handlem
    // 0x25: Destroy thread
    .quad       _ZN3sys13ThreadDestroyE6Handle
    // 0x26: Set thread state 
    .quad       _ZN3sys7Syscall20UnimplementedSyscallEv
    // 0x27: Set thread priority
    .quad       _ZN3sys17ThreadSetPriorityE6Handlel
    // 0x28: Set thread notification mask
    .quad       _ZN3sys17ThreadSetNoteMaskE6Handlem
    // 0x29: Set thread name
    .quad       _ZN3sys13ThreadSetNameE6HandlePKcm
    // 0x2A: resume thread
    .quad       _ZN3sys12ThreadResumeE6Handle

    // 0x2B-0x2F: reserved
    .rept       5
    .quad       _ZN3sys7Syscall20UnimplementedSyscallEv
    .endr

    // 0x30: Get task handle
    .quad       _ZN3sys13TaskGetHandleEv
    // 0x31: Create task
    .quad       _ZN3sys10TaskCreateE6Handle
    // 0x32: Terminate task
    .quad       _ZN3sys13TaskTerminateE6Handlel
    // 0x33: Initialize task
    .quad       _ZN3sys14TaskInitializeE6Handlemm
    // 0x34: Set task name
    .quad       _ZN3sys11TaskSetNameE6HandlePKcm
    // 0x35: Wait on task
    .quad       _ZN3sys7Syscall20UnimplementedSyscallEv
    // 0x36: Debug printing
    .quad       _ZN3sys10TaskDbgOutEPKcm

    // 0x37: Reserved
    .quad       _ZN3sys7Syscall20UnimplementedSyscallEv

    // 0x38: Register IRQ handler
    .quad       _ZN3sys17IrqHandlerInstallEm6Handlem
    // 0x39: Unregister IRQ handler
    .quad       _ZN3sys16IrqHandlerRemoveE6Handle
    // 0x3A: Update IRQ handler
    .quad       _ZN3sys16IrqHandlerUpdateE6HandleS0_m
    // 0x3B: Get IRQ handler information
    .quad       _ZN3sys17IrqHandlerGetInfoE6Handlem
    // 0x3C: Allocate processor local IRQ handler
    .quad       _ZN3sys24IrqHandlerAllocCoreLocalE6Handlem

    // 0x3D-0x3F: Reserved
    .rept       3
    .quad       _ZN3sys7Syscall20UnimplementedSyscallEv
    .endr

    // 0x40: amd64: Set thread local base address
    .quad       _ZN4arch7syscall19UpdateThreadTlsBaseEmbm
    // 0x41: amd64: Get bootloader info
    .quad       _ZN4arch7syscall13GetLoaderInfoEPvm
    // 0x42: amd64: Update IO permission bitmap
    .quad       _ZN4arch7syscall18UpdateIoPermissionE6HandlePKhmmm
    // 0x43: amd64: Lock IO permission bitmap
    .quad       _ZN4arch7syscall16LockIoPermissionE6Handle
    // 0x44: amd64: Read from IO port
    .quad       _ZN4arch7syscall10IoPortReadEmmPj
    // 0x45: amd64: Write to IO port
    .quad       _ZN4arch7syscall11IoPortWriteEmmj

arch_syscall_table_end:

/**
 * The syscall number is passed in %rax.
 *
 * Up to six arguments are passed in the registers RDI, RSI, RDX, R10, R8, and R9, in order. This
 * is almost identical to the standard SysV calling convention, with the exception that we use R10
 * for the fourth argument rather than RCX as that's clobbered by the SYSCALL instruction.
 *
 * The SYSCALL in user mode will have set RCX to the return address and R11 to the flags to restore
 * on return. We'll return with the value of R9 set into the stack pointer.
 */
.global arch_syscall_entry
.align 0x40 // align to cache line
arch_syscall_entry:
    // acquire the per-CPU information and get stack
    swapgs
    movq        %gs:PROCI_OFF_SYSCALL_STACK, %rsp

    sti

    // save registers
    state_save
    xor         %rbp, %rbp

    // otherwise, bounds check and jump to handler
    cmp         $(arch_syscall_table_end - arch_syscall_table)/8, %rax
    jge         .invalidSyscall

    mov         %r10, %rcx // fix the 4th argument

    lea         arch_syscall_table(%rip), %rbx
    call        *(%rbx, %rax, 8)

    // fall through to exit handler

/**
 * Common return handler for syscalls.
 *
 * At the time this is called, the registers should have been restored to the state they were in
 * at the time the syscall was made. We just have to restore %gs base, the user's stack, and then
 * return back to userland.
 *
 * The instruction will fault if we try to return to a page marked as supervisor-only access, or
 * use a stack that's not mapped for that process.
 */
arch_syscall_exit:
    state_restore

    // restore gs
    cli
    swapgs

    // return from syscall: R11 and RCX are the SYSCALL values
    sysretq

.invalidSyscall:
    mov         $-5, %rax
    jmp         arch_syscall_exit
