#include "exceptions.h"
#include "idt.h"

#include <printf.h>
#include <log.h>

// define the assembly exception handlers
extern void x86_exception_div0();
extern void x86_exception_debug();
extern void x86_exception_nmi();
extern void x86_exception_breakpoint();
extern void x86_exception_overflow();
extern void x86_exception_bounds();
extern void x86_exception_invalid_instruction();
extern void x86_exception_device_unavailable();
extern void x86_exception_double_fault();
extern void x86_exception_tss_invalid();
extern void x86_exception_segment_missing();
extern void x86_exception_ss_invalid();
extern void x86_exception_gpf();
extern void x86_exception_pagefault();
extern void x86_exception_float();
extern void x86_exception_alignment_check();
extern void x86_exception_machine_check();
extern void x86_exception_simd();
extern void x86_exception_virtualization();

/**
 * Installs the default set of exception handlers.
 */
void exception_install_handlers() {
    idt_set_entry(0, (uintptr_t) x86_exception_div0, 0x08, 0x8E);
    idt_set_entry(1, (uintptr_t) x86_exception_debug, 0x08, 0x8E);
    idt_set_entry(2, (uintptr_t) x86_exception_nmi, 0x08, 0x8E);
    idt_set_entry(3, (uintptr_t) x86_exception_breakpoint, 0x08, 0x8E);
    idt_set_entry(4, (uintptr_t) x86_exception_overflow, 0x08, 0x8E);
    idt_set_entry(5, (uintptr_t) x86_exception_bounds, 0x08, 0x8E);
    idt_set_entry(6, (uintptr_t) x86_exception_invalid_instruction, 0x08, 0x8E);
    idt_set_entry(7, (uintptr_t) x86_exception_device_unavailable, 0x08, 0x8E);
    idt_set_entry(8, (uintptr_t) x86_exception_double_fault, 0x08, 0x8E);
    // vector 9 is for coprocessor segment overrun; since 486 we'd get a #GP instead
    idt_set_entry(10, (uintptr_t) x86_exception_tss_invalid, 0x08, 0x8E);
    idt_set_entry(11, (uintptr_t) x86_exception_segment_missing, 0x08, 0x8E);
    idt_set_entry(12, (uintptr_t) x86_exception_ss_invalid, 0x08, 0x8E);
    idt_set_entry(13, (uintptr_t) x86_exception_gpf, 0x08, 0x8E);
    idt_set_entry(14, (uintptr_t) x86_exception_pagefault, 0x08, 0x8E);
    // vector 15 is reserved
    idt_set_entry(16, (uintptr_t) x86_exception_float, 0x08, 0x8E);
    idt_set_entry(17, (uintptr_t) x86_exception_alignment_check, 0x08, 0x8E);
    idt_set_entry(18, (uintptr_t) x86_exception_machine_check, 0x08, 0x8E);
    idt_set_entry(19, (uintptr_t) x86_exception_simd, 0x08, 0x8E);
    idt_set_entry(20, (uintptr_t) x86_exception_virtualization, 0x08, 0x8E);
}

/**
 * Formats an x86 exception info blob.
 *
 * @return Number of characters written
 */
int x86_exception_format_info(char *outBuf, const size_t outBufLen,
        const x86_exception_info_t info) {
    return snprintf(outBuf, outBufLen, "Exception %3lu ($%08lx)\n"
            " CS $%08lx  DS $%08lx  SS $%08lx\n"
            "EAX $%08lx EBX $%08lx EDX $%08lx ECX $%08lx\n"
            "EDI $%08lx ESI $%08lx EBP $%08lx ESP $%08lx\n"
            "EIP $%08lx USP $%08lx ESP $%08lx EFLAGS $%08lx",
            info.intNo, info.errCode, info.cs, info.ds, info.ss,
            info.eax, info.ebx, info.ecx, info.edx, info.edi, info.esi, info.ebp, info.esp,
            info.eip, info.useresp, info.esp, info.eflags
    );
}

/**
 * Handles a page fault exception.
 */
void x86_handle_pagefault(const x86_exception_info_t info) {
    uint32_t faultAddr;
    asm volatile("mov %%cr2, %0" : "=r" (faultAddr));

    // get some info on the fault

    // page fault is unhandled
    char buf[512] = {0};
    x86_exception_format_info(buf, 512, info);
    panic("unhandled page fault: %s %s (%s) at $%08lx\n%s", 
            ((info.errCode & 0x04) ? "user" : "supervisor"),
            ((info.errCode & 0x02) ? "write" : "read"),
            ((info.errCode & 0x01) ? "present" : "not present"),
            faultAddr, buf);
}

