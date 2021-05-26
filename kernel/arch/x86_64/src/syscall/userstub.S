.global user_syscall_stub

.section .text.first, "ax"
/**
 * Userspace syscall stub
 *
 * This is up to 1 page of code that's mapped into every userspace process at a fixed memory
 * address; it is used to make syscalls.
 *
 * The exact layout of the syscall stub is required by the kernel, such that it knows the return
 * address for SYSEXIT ahead of time. This saves us a register that we can instead use to pass
 * parameters.
 */
user_syscall_stub:
    push        %rbp

    // the kernel will restore stack from %ebp
    mov         %rsp, %rbp
    syscall

user_syscall_stub_return:
    pop         %rbp
    ret
