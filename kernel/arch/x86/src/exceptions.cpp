#include "exceptions.h"
#include "exception_types.h"

#include "idt.h"

#include <printf.h>
#include <log.h>

/**
 * Mapping of exception number to name
 */
static const struct {
    uint8_t vector;
    const char *name;
} x86_exception_name[] = {
    {X86_EXC_DIVIDE, "Divide-by-zero"},
    {X86_EXC_DEBUG, "Debug"},
    {X86_EXC_NMI, "Non-Maskable Interrupt"},
    {X86_EXC_BREAKPOINT, "Breakpoint"},
    {X86_EXC_OVERFLOW, "Overflow"},
    {X86_EXC_BOUNDS, "Bound range exceeded"},
    {X86_EXC_ILLEGAL_OPCODE, "Invalid instruction"},
    {X86_EXC_DEVICE_UNAVAIL, "Device unavailable (FPU)"},
    {X86_EXC_DOUBLE_FAULT, "Double fault"},
    {X86_EXC_INVALID_TSS, "Invalid TSS"},
    {X86_EXC_SEGMENT_NP, "Segment not present"},
    {X86_EXC_SS, "Invalid stack segment"},
    {X86_EXC_GPF, "General protection fault"},
    {X86_EXC_PAGING, "Page fault"},
    {X86_EXC_FP, "Floating point exception"},
    {X86_EXC_ALIGNMENT, "Alignment check"},
    {X86_EXC_MCE, "Machine check"},
    {X86_EXC_SIMD_FP, "SIMD float exception"},
    {X86_EXC_VIRT, "Virtualization exception"},

    // these are ones we should never get
    {15, "Reserved"},
    {0xFF, NULL},
};

/// Return a display name for the given exception
static const char *vector_name(const uint8_t vector) {
    size_t i = 0;
    while(x86_exception_name[i].name) {
        if(x86_exception_name[i].vector == vector) {
            return x86_exception_name[i].name;
        }

        i++;
    }

    return "Unknown";
};

// define the assembly exception handlers
extern "C" void x86_exception_div0();
extern "C" void x86_exception_debug();
extern "C" void x86_exception_nmi();
extern "C" void x86_exception_breakpoint();
extern "C" void x86_exception_overflow();
extern "C" void x86_exception_bounds();
extern "C" void x86_exception_invalid_instruction();
extern "C" void x86_exception_device_unavailable();
extern "C" void x86_exception_double_fault();
extern "C" void x86_exception_tss_invalid();
extern "C" void x86_exception_segment_missing();
extern "C" void x86_exception_ss_invalid();
extern "C" void x86_exception_gpf();
extern "C" void x86_exception_pagefault();
extern "C" void x86_exception_float();
extern "C" void x86_exception_alignment_check();
extern "C" void x86_exception_machine_check();
extern "C" void x86_exception_simd();
extern "C" void x86_exception_virtualization();



/**
 * Installs the default set of exception handlers.
 */
void exception_install_handlers() {
    idt_set_entry(X86_EXC_DIVIDE, (uintptr_t) x86_exception_div0, 0x08, 0x8E);
    idt_set_entry(X86_EXC_DEBUG, (uintptr_t) x86_exception_debug, 0x08, 0x8E);
    idt_set_entry(X86_EXC_NMI, (uintptr_t) x86_exception_nmi, 0x08, 0x8E);
    idt_set_entry(X86_EXC_BREAKPOINT, (uintptr_t) x86_exception_breakpoint, 0x08, 0x8E);
    idt_set_entry(X86_EXC_OVERFLOW, (uintptr_t) x86_exception_overflow, 0x08, 0x8E);
    idt_set_entry(X86_EXC_BOUNDS, (uintptr_t) x86_exception_bounds, 0x08, 0x8E);
    idt_set_entry(X86_EXC_ILLEGAL_OPCODE, (uintptr_t) x86_exception_invalid_instruction, 0x08, 0x8E);
    idt_set_entry(X86_EXC_DEVICE_UNAVAIL, (uintptr_t) x86_exception_device_unavailable, 0x08, 0x8E);
    idt_set_entry(X86_EXC_DOUBLE_FAULT, (uintptr_t) x86_exception_double_fault, 0x08, 0x8E);
    idt_set_entry(X86_EXC_INVALID_TSS, (uintptr_t) x86_exception_tss_invalid, 0x08, 0x8E);
    idt_set_entry(X86_EXC_SEGMENT_NP, (uintptr_t) x86_exception_segment_missing, 0x08, 0x8E);
    idt_set_entry(X86_EXC_SS, (uintptr_t) x86_exception_ss_invalid, 0x08, 0x8E);
    idt_set_entry(X86_EXC_GPF, (uintptr_t) x86_exception_gpf, 0x08, 0x8E);
    idt_set_entry(X86_EXC_PAGING, (uintptr_t) x86_exception_pagefault, 0x08, 0x8E);
    idt_set_entry(X86_EXC_FP, (uintptr_t) x86_exception_float, 0x08, 0x8E);
    idt_set_entry(X86_EXC_ALIGNMENT, (uintptr_t) x86_exception_alignment_check, 0x08, 0x8E);
    idt_set_entry(X86_EXC_MCE, (uintptr_t) x86_exception_machine_check, 0x08, 0x8E);
    idt_set_entry(X86_EXC_SIMD_FP, (uintptr_t) x86_exception_simd, 0x08, 0x8E);
    idt_set_entry(X86_EXC_VIRT, (uintptr_t) x86_exception_virtualization, 0x08, 0x8E);
}

/**
 * Formats an x86 exception info blob.
 *
 * @return Number of characters written
 */
int x86_exception_format_info(char *outBuf, const size_t outBufLen,
        const x86_exception_info_t &info) {
    return snprintf(outBuf, outBufLen, "Exception %3lu ($%08lx)\n"
            " CS $%08lx  DS $%08lx  ES $%08lx  FS $%08lx\n"
            " GS $%08lx  SS $%08lx\n"
            "EAX $%08lx EBX $%08lx ECX $%08lx EDX $%08lx\n"
            "EDI $%08lx ESI $%08lx EBP $%08lx ESP $%08lx\n"
            "EIP $%08lx EFLAGS $%08lx",
            info.intNo, info.errCode,
            info.cs, info.ds, info.es, info.fs, 
            info.gs, info.ss,
            info.eax, info.ebx, info.ecx, info.edx,
            info.edi, info.esi, info.ebp, info.esp,
            info.eip, info.eflags
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
    panic("unhandled page fault: %s%s %s (%s) at $%08lx\n%s", 
            ((info.errCode & 0x08) ? "reserved bit violation on " : ""),
            ((info.errCode & 0x04) ? "user" : "supervisor"),
            ((info.errCode & 0x02) ? "write" : "read"),
            ((info.errCode & 0x01) ? "present" : "not present"),
            faultAddr, buf);
}



/**
 * Exception handler; routes the exception into the correct part of the kernel.
 */
void x86_handle_exception(const x86_exception_info_t info) {
    char buf[512] = {0};
    x86_exception_format_info(buf, 512, info);
    panic("unhandled exception: %s\n%s", vector_name(info.intNo), buf);
}

