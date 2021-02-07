/// Macro to save state for syscall
.macro state_save
    push        %ebp
    push        %ecx
    push        %edx
.endm
/// Macro to restore syscall state, companion to state_save
.macro state_restore
    pop         %edx
    pop         %ecx
    pop         %ebp
.endm

/**
 * Entry point for syscalls; we'll quickly see if the call in question is fast IPC, or whether we
 * fall back to the slow C/C++ handler path.
 *
 * The syscall number is passed in %ax; values with the high bit set take the slow path. The high
 * 16 bits of %eax are reserved for syscall-specific use. On return, %eax contains the syscall
 * status code.
 */
.global arch_syscall_entry
.align 0x40 // align to cache line
arch_syscall_entry:
    // test for slow path
    bt          %ax, 15
    jc          arch_syscall_slow

arch_msgsend_fast:
    // get destination port

    // is there a thread blocking on it?

    // context switch to it

/**
 * Slow path for message sending
 */
arch_msgsend_slow:
    state_save

    // restore state and exit
    state_restore
    jmp         arch_syscall_exit

/**
 * Handles a "regular" syscall that goes through the normal dispatching mechanism.
 */
arch_syscall_slow:
    // save registers
    state_save
    xor         %ebp, %ebp

    // push syscall info and invoke the handler
    push        %eax
    call        arch_syscall_handle

    add         $4, %esp

    // restore state and exit
    state_restore
    jmp         arch_syscall_exit

/**
 * Common return handler for syscalls.
 *
 * We don't actually really have to do anything here; once we jump here, the register state has
 * been restored to exactly how it was when we entered the syscall, meaning that both %ecx (the
 * desired ring 3 stack pointer value) and %edx (ring 3 return address) are what the syscall stub
 * in userspace set them to.
 *
 * The instruction will fault if we try to return to a page marked as supervisor-only access, or
 * use a stack that's not mapped for that process.
 */
arch_syscall_exit:
    sysexit
