#include "exceptions.h"
#include "idt.h"

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
    idt_set_entry(0, (uint32_t) x86_exception_div0, 0x08, 0x8E);
    idt_set_entry(1, (uint32_t) x86_exception_debug, 0x08, 0x8E);
    idt_set_entry(2, (uint32_t) x86_exception_nmi, 0x08, 0x8E);
    idt_set_entry(3, (uint32_t) x86_exception_breakpoint, 0x08, 0x8E);
    idt_set_entry(4, (uint32_t) x86_exception_overflow, 0x08, 0x8E);
    idt_set_entry(5, (uint32_t) x86_exception_bounds, 0x08, 0x8E);
    idt_set_entry(6, (uint32_t) x86_exception_invalid_instruction, 0x08, 0x8E);
    idt_set_entry(7, (uint32_t) x86_exception_device_unavailable, 0x08, 0x8E);
    idt_set_entry(8, (uint32_t) x86_exception_double_fault, 0x08, 0x8E);
    // vector 9 is for coprocessor segment overrun; since 486 we'd get a #GP instead
    idt_set_entry(10, (uint32_t) x86_exception_tss_invalid, 0x08, 0x8E);
    idt_set_entry(11, (uint32_t) x86_exception_segment_missing, 0x08, 0x8E);
    idt_set_entry(12, (uint32_t) x86_exception_ss_invalid, 0x08, 0x8E);
    idt_set_entry(13, (uint32_t) x86_exception_gpf, 0x08, 0x8E);
    idt_set_entry(14, (uint32_t) x86_exception_pagefault, 0x08, 0x8E);
    // vector 15 is reserved
    idt_set_entry(16, (uint32_t) x86_exception_float, 0x08, 0x8E);
    idt_set_entry(17, (uint32_t) x86_exception_alignment_check, 0x08, 0x8E);
    idt_set_entry(18, (uint32_t) x86_exception_machine_check, 0x08, 0x8E);
    idt_set_entry(19, (uint32_t) x86_exception_simd, 0x08, 0x8E);
    idt_set_entry(20, (uint32_t) x86_exception_virtualization, 0x08, 0x8E);
}
