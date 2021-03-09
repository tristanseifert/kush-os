#include "exceptions.h"
#include "exception_types.h"

#include "gdt.h"
#include "idt.h"

#include <sched/Thread.h>
#include <vm/Map.h>

#include <printf.h>
#include <log.h>

#include <arch/x86_msr.h>


using namespace arch;

/**
 * Mapping of exception number to name
 */
static const struct {
    uint8_t vector;
    const char *name;
} amd64_exception_name[] = {
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
    while(amd64_exception_name[i].name) {
        if(amd64_exception_name[i].vector == vector) {
            return amd64_exception_name[i].name;
        }

        i++;
    }

    return "Unknown";
};

// define the assembly exception handlers
extern "C" void amd64_exception_div0();
extern "C" void amd64_exception_debug();
extern "C" void amd64_exception_nmi();
extern "C" void amd64_exception_breakpoint();
extern "C" void amd64_exception_overflow();
extern "C" void amd64_exception_bounds();
extern "C" void amd64_exception_invalid_instruction();
extern "C" void amd64_exception_device_unavailable();
extern "C" void amd64_exception_double_fault();
extern "C" void amd64_exception_tss_invalid();
extern "C" void amd64_exception_segment_missing();
extern "C" void amd64_exception_ss_invalid();
extern "C" void amd64_exception_gpf();
extern "C" void amd64_exception_pagefault();
extern "C" void amd64_exception_float();
extern "C" void amd64_exception_alignment_check();
extern "C" void amd64_exception_machine_check();
extern "C" void amd64_exception_simd();
extern "C" void amd64_exception_virtualization();



/**
 * Installs the default set of exception handlers.
 */
void exception_install_handlers() {
    Idt::Set(X86_EXC_DIVIDE, (uintptr_t) amd64_exception_div0, GDT_KERN_CODE_SEG, IDT_FLAGS_TRAP, Idt::Stack::Stack1);
    Idt::Set(X86_EXC_DEBUG, (uintptr_t) amd64_exception_debug, GDT_KERN_CODE_SEG, IDT_FLAGS_TRAP, Idt::Stack::Stack4);
    Idt::Set(X86_EXC_NMI, (uintptr_t) amd64_exception_nmi, GDT_KERN_CODE_SEG, IDT_FLAGS_ISR, Idt::Stack::Stack3);
    Idt::Set(X86_EXC_BREAKPOINT, (uintptr_t) amd64_exception_breakpoint, GDT_KERN_CODE_SEG, IDT_FLAGS_TRAP, Idt::Stack::Stack4);
    Idt::Set(X86_EXC_OVERFLOW, (uintptr_t) amd64_exception_overflow, GDT_KERN_CODE_SEG, IDT_FLAGS_TRAP, Idt::Stack::Stack1);
    Idt::Set(X86_EXC_BOUNDS, (uintptr_t) amd64_exception_bounds, GDT_KERN_CODE_SEG, IDT_FLAGS_TRAP, Idt::Stack::Stack1);
    Idt::Set(X86_EXC_ILLEGAL_OPCODE, (uintptr_t) amd64_exception_invalid_instruction, GDT_KERN_CODE_SEG, IDT_FLAGS_TRAP, Idt::Stack::Stack2);
    Idt::Set(X86_EXC_DEVICE_UNAVAIL, (uintptr_t) amd64_exception_device_unavailable, GDT_KERN_CODE_SEG, IDT_FLAGS_TRAP, Idt::Stack::Stack2);
    Idt::Set(X86_EXC_DOUBLE_FAULT, (uintptr_t) amd64_exception_double_fault, GDT_KERN_CODE_SEG, IDT_FLAGS_TRAP, Idt::Stack::Stack2);
    Idt::Set(X86_EXC_INVALID_TSS, (uintptr_t) amd64_exception_tss_invalid, GDT_KERN_CODE_SEG, IDT_FLAGS_TRAP, Idt::Stack::Stack1);
    Idt::Set(X86_EXC_SEGMENT_NP, (uintptr_t) amd64_exception_segment_missing, GDT_KERN_CODE_SEG, IDT_FLAGS_TRAP, Idt::Stack::Stack1);
    Idt::Set(X86_EXC_SS, (uintptr_t) amd64_exception_ss_invalid, GDT_KERN_CODE_SEG, IDT_FLAGS_TRAP, Idt::Stack::Stack1);
    Idt::Set(X86_EXC_GPF, (uintptr_t) amd64_exception_gpf, GDT_KERN_CODE_SEG, IDT_FLAGS_TRAP, Idt::Stack::Stack2);
    Idt::Set(X86_EXC_PAGING, (uintptr_t) amd64_exception_pagefault, GDT_KERN_CODE_SEG, IDT_FLAGS_TRAP, Idt::Stack::Stack2);
    Idt::Set(X86_EXC_FP, (uintptr_t) amd64_exception_float, GDT_KERN_CODE_SEG, IDT_FLAGS_TRAP, Idt::Stack::Stack2);
    Idt::Set(X86_EXC_ALIGNMENT, (uintptr_t) amd64_exception_alignment_check, GDT_KERN_CODE_SEG, IDT_FLAGS_TRAP, Idt::Stack::Stack2);
    Idt::Set(X86_EXC_MCE, (uintptr_t) amd64_exception_machine_check, GDT_KERN_CODE_SEG, IDT_FLAGS_TRAP, Idt::Stack::Stack4);
    Idt::Set(X86_EXC_SIMD_FP, (uintptr_t) amd64_exception_simd, GDT_KERN_CODE_SEG, IDT_FLAGS_TRAP, Idt::Stack::Stack1);
    Idt::Set(X86_EXC_VIRT, (uintptr_t) amd64_exception_virtualization, GDT_KERN_CODE_SEG, IDT_FLAGS_TRAP, Idt::Stack::Stack1);
}

/**
 * Formats an x86_64 exception info blob.
 *
 * @return Number of characters written
 */
int amd64_exception_format_info(char *outBuf, const size_t outBufLen,
        const amd64_exception_info_t *info) {
    uint64_t cr0, cr2, cr3;
    asm volatile("mov %%cr0, %0" : "=r" (cr0));
    asm volatile("mov %%cr2, %0" : "=r" (cr2));
    asm volatile("mov %%cr3, %0" : "=r" (cr3));

    /*
    uint64_t xmm[8][2];
    memcpy(&xmm, &info->xmm, 16 * 8);
*/

    // get the FS/GS base
    uint32_t tempLo, tempHi;
    uint64_t gsBase = 0, fsBase = 0, gsKernBase = 0;

    x86_msr_read(X86_MSR_FSBASE, &tempLo, &tempHi);
    fsBase = (tempLo) | (static_cast<uint64_t>(tempHi) << 32);

    x86_msr_read(X86_MSR_GSBASE, &tempLo, &tempHi);
    gsBase = (tempLo) | (static_cast<uint64_t>(tempHi) << 32);
    x86_msr_read(X86_MSR_KERNEL_GSBASE, &tempLo, &tempHi);
    gsKernBase = (tempLo) | (static_cast<uint64_t>(tempHi) << 32);

    // format
    return snprintf(outBuf, outBufLen, "Exception %3llu ($%016llx)\n"
            "CR0 $%016llx CR2 $%016llx CR3 $%016llx\n"
            " CS $%04x SS $%04x RFLAGS $%016llx\n"
            " FS $%016llx  GS $%016llx KGS $%016llx\n"
            "RAX $%016llx RBX $%016llx RCX $%016llx RDX $%016llx\n"
            "RDI $%016llx RSI $%016llx RBP $%016llx RSP $%016llx\n"
            " R8 $%016llx  R9 $%016llx R10 $%016llx R11 $%016llx\n"
            "R12 $%016llx R13 $%016llx R14 $%016llx R15 $%016llx\n"
            "RIP $%016llx\n"
            /*"XMM0 $%016llx%016llx\n"
            "XMM1 $%016llx%016llx\n"
            "XMM2 $%016llx%016llx\n"
            "XMM3 $%016llx%016llx\n"
            "XMM4 $%016llx%016llx\n"
            "XMM5 $%016llx%016llx\n"
            "XMM6 $%016llx%016llx\n"
            "XMM7 $%016llx%016llx\n"*/
            ,
            info->intNo, info->errCode,
            cr0, cr2, cr3,
            info->cs, info->ss, info->rflags,
            fsBase, gsBase,
            info->rax, info->rbx, info->rcx, info->rdx,
            info->rdi, info->rsi, info->rbp, info->rsp,
            info->r8, info->r9, info->r10, info->r11,
            info->r12, info->r13, info->r14, info->r15,
            info->rip/*,
            xmm[0][1], xmm[0][0], xmm[1][1], xmm[1][0],
            xmm[2][1], xmm[2][0], xmm[3][1], xmm[3][0],
            xmm[4][1], xmm[4][0], xmm[5][1], xmm[5][0],
            xmm[6][1], xmm[6][0], xmm[7][1], xmm[7][0]*/
    );
}

/**
 * Handles a page fault exception.
 */
void amd64_handle_pagefault(amd64_exception_info_t *info) {
    // get some info on the fault
    uint64_t faultAddr;
    asm volatile("mov %%cr2, %0" : "=r" (faultAddr));


    // if the fault is a reserved bit violation, fail immediately
    if(info->errCode & 0x08) {
        goto unhandled;
    }

    // forward userspace page faults to the VM manager
    if(faultAddr < 0x8000000000000000 && (info->errCode & 0x04)) {
        auto vm = ::vm::Map::current();
        bool handled = vm->handlePagefault(faultAddr, (info->errCode & 0x01), (info->errCode & 0x02));

        if(handled) {
            return;
        }

        // it wasn't handled by the VM manager; let the thread handle it
        auto thread = sched::Thread::current();
        if(thread) {
            thread->handleFault(sched::Thread::FaultType::UnhandledPagefault, faultAddr, &info->rip, &info);
            return;
        }
    }

unhandled:;

    // page fault is unhandled (or in kernel)
    constexpr static const size_t kBufSz = 1024;
    char buf[kBufSz] = {0};
    amd64_exception_format_info(buf, kBufSz, info);
    panic("unhandled page fault: %s%s %s (%s) at $%016llx\n%s", 
            ((info->errCode & 0x08) ? "reserved bit violation on " : ""),
            ((info->errCode & 0x04) ? "user" : "supervisor"),
            ((info->errCode & 0x02) ? "write" : "read"),
            ((info->errCode & 0x01) ? "present" : "not present"),
            faultAddr, buf);
}



/**
 * Exception handler; routes the exception into the correct part of the kernel.
 */
void amd64_handle_exception(amd64_exception_info_t *info) {
    // acquire currently executing thread
    auto thread = sched::Thread::current();
    if(!thread) goto unhandled;

    // if exception is in kernel space, always panic
    if(info->rip >= 0x8000000000000000) goto unhandled;

    // try to handle exception
    switch(info->intNo) {
        case X86_EXC_ILLEGAL_OPCODE:
            thread->handleFault(sched::Thread::FaultType::InvalidInstruction, info->rip, &info->rip, info);
            break;
        case X86_EXC_GPF:
            thread->handleFault(sched::Thread::FaultType::ProtectionViolation, info->rip, &info->rip, info);
            break;

        default:
            goto unhandled;
            break;
    }

    // if we get here, the error was handled and we can just return to it
    return;

unhandled:;
    // otherwise, panique
    constexpr static const size_t kBufSz = 1024;
    char buf[kBufSz] = {0};
    amd64_exception_format_info(buf, kBufSz, info);
    panic("unhandled exception: %s\n%s", vector_name(info->intNo), buf);
}

/**
 * Arch print handler
 */
int arch::PrintState(const void *_state, char *buf, const size_t bufLen) {
    const auto state = reinterpret_cast<const amd64_exception_info_t *>(_state);
    amd64_exception_format_info(buf, bufLen, state);

    return 0;
}
