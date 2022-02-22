#include "ExceptionHandlers.h"
#include "Gdt.h"
#include "Idt.h"

#include <Exceptions/Handler.h>
#include <Logging/Console.h>

extern "C" {
void amd64_exception_div0();
void amd64_exception_debug();
void amd64_exception_nmi();
void amd64_exception_breakpoint();
void amd64_exception_overflow();
void amd64_exception_bounds();
void amd64_exception_invalid_instruction();
void amd64_exception_device_unavailable();
void amd64_exception_double_fault();
void amd64_exception_tss_invalid();
void amd64_exception_segment_missing();
void amd64_exception_ss_invalid();
void amd64_exception_gpf();
void amd64_exception_pagefault();
void amd64_exception_float();
void amd64_exception_alignment_check();
void amd64_exception_machine_check();
void amd64_exception_simd();
void amd64_exception_virtualization();
}

using namespace Platform::Amd64Uefi;

/**
 * Mapping of exception number to name
 */
static const struct {
    uint8_t vector;
    const char *name;
} gExceptionNames[] = {
    {AMD64_EXC_DIVIDE, "Divide-by-zero"},
    {AMD64_EXC_DEBUG, "Debug"},
    {AMD64_EXC_NMI, "Non-Maskable Interrupt"},
    {AMD64_EXC_BREAKPOINT, "Breakpoint"},
    {AMD64_EXC_OVERFLOW, "Overflow"},
    {AMD64_EXC_BOUNDS, "Bound range exceeded"},
    {AMD64_EXC_ILLEGAL_OPCODE, "Invalid instruction"},
    {AMD64_EXC_DEVICE_UNAVAIL, "Device unavailable (FPU)"},
    {AMD64_EXC_DOUBLE_FAULT, "Double fault"},
    {AMD64_EXC_INVALID_TSS, "Invalid TSS"},
    {AMD64_EXC_SEGMENT_NP, "Segment not present"},
    {AMD64_EXC_SS, "Invalid stack segment"},
    {AMD64_EXC_GPF, "General protection fault"},
    {AMD64_EXC_PAGING, "Page fault"},
    {AMD64_EXC_FP, "Floating point exception"},
    {AMD64_EXC_ALIGNMENT, "Alignment check"},
    {AMD64_EXC_MCE, "Machine check"},
    {AMD64_EXC_SIMD_FP, "SIMD float exception"},
    {AMD64_EXC_VIRT, "Virtualization exception"},

    // these are ones we should never get
    {15, "Reserved"},
    {0xFF, NULL},
};

/**
 * Convert an exception number (the `errorCode` field in the regs structure) to a name.
 */
static const char *GetExceptionName(const uint8_t vector) {
    size_t i = 0;
    while(gExceptionNames[i].name) {
        const auto &record = gExceptionNames[i];

        if(record.vector == vector) {
            return record.name;
        }

        i++;
    }

    return "Unknown";
};

/**
 * Install all of the default exception handlers in the IDT specified.
 *
 * @param idt Interrupt descriptor table to receive the default exception vectors; they will be
 *        written to the first 32 vectors.
 */
void ExceptionHandlers::Install(Idt &idt) {
    idt.set(AMD64_EXC_DIVIDE, reinterpret_cast<uintptr_t>(amd64_exception_div0), GDT_KERN_CODE_SEG,
            Idt::kTrapFlags, Idt::Stack::Stack1);
    idt.set(AMD64_EXC_DEBUG, reinterpret_cast<uintptr_t>(amd64_exception_debug), GDT_KERN_CODE_SEG,
            Idt::kTrapFlags, Idt::Stack::Stack4);
    idt.set(AMD64_EXC_NMI, reinterpret_cast<uintptr_t>(amd64_exception_nmi), GDT_KERN_CODE_SEG,
                Idt::kIsrFlags, Idt::Stack::Stack3);
    idt.set(AMD64_EXC_BREAKPOINT, reinterpret_cast<uintptr_t>(amd64_exception_breakpoint),
        GDT_KERN_CODE_SEG, Idt::kTrapFlags, Idt::Stack::Stack4);
    idt.set(AMD64_EXC_OVERFLOW, reinterpret_cast<uintptr_t>(amd64_exception_overflow),
        GDT_KERN_CODE_SEG, Idt::kTrapFlags, Idt::Stack::Stack1);
    idt.set(AMD64_EXC_BOUNDS, reinterpret_cast<uintptr_t>(amd64_exception_bounds),
        GDT_KERN_CODE_SEG, Idt::kTrapFlags, Idt::Stack::Stack1);
    idt.set(AMD64_EXC_ILLEGAL_OPCODE,
        reinterpret_cast<uintptr_t>(amd64_exception_invalid_instruction), GDT_KERN_CODE_SEG,
        Idt::kTrapFlags, Idt::Stack::Stack2);
    idt.set(AMD64_EXC_DEVICE_UNAVAIL,
        reinterpret_cast<uintptr_t>(amd64_exception_device_unavailable), GDT_KERN_CODE_SEG,
        Idt::kTrapFlags, Idt::Stack::Stack2);
    idt.set(AMD64_EXC_DOUBLE_FAULT, reinterpret_cast<uintptr_t>(amd64_exception_double_fault),
        GDT_KERN_CODE_SEG, Idt::kTrapFlags, Idt::Stack::Stack2);
    idt.set(AMD64_EXC_INVALID_TSS, reinterpret_cast<uintptr_t>(amd64_exception_tss_invalid),
        GDT_KERN_CODE_SEG, Idt::kTrapFlags, Idt::Stack::Stack1);
    idt.set(AMD64_EXC_SEGMENT_NP, reinterpret_cast<uintptr_t>(amd64_exception_segment_missing),
        GDT_KERN_CODE_SEG, Idt::kTrapFlags, Idt::Stack::Stack1);
    idt.set(AMD64_EXC_SS, reinterpret_cast<uintptr_t>(amd64_exception_ss_invalid),
        GDT_KERN_CODE_SEG, Idt::kTrapFlags, Idt::Stack::Stack1);
    idt.set(AMD64_EXC_GPF, reinterpret_cast<uintptr_t>(amd64_exception_gpf), GDT_KERN_CODE_SEG,
            Idt::kTrapFlags, Idt::Stack::Stack2);
    idt.set(AMD64_EXC_PAGING, reinterpret_cast<uintptr_t>(amd64_exception_pagefault),
            GDT_KERN_CODE_SEG, Idt::kTrapFlags, Idt::Stack::Stack2);
    idt.set(AMD64_EXC_FP, reinterpret_cast<uintptr_t>(amd64_exception_float), GDT_KERN_CODE_SEG,
            Idt::kTrapFlags, Idt::Stack::Stack2);
    idt.set(AMD64_EXC_ALIGNMENT, reinterpret_cast<uintptr_t>(amd64_exception_alignment_check),
            GDT_KERN_CODE_SEG, Idt::kTrapFlags, Idt::Stack::Stack2);
    idt.set(AMD64_EXC_MCE, reinterpret_cast<uintptr_t>(amd64_exception_machine_check),
            GDT_KERN_CODE_SEG, Idt::kTrapFlags, Idt::Stack::Stack4);
    idt.set(AMD64_EXC_SIMD_FP, reinterpret_cast<uintptr_t>(amd64_exception_simd),
            GDT_KERN_CODE_SEG, Idt::kTrapFlags, Idt::Stack::Stack1);
    idt.set(AMD64_EXC_VIRT, reinterpret_cast<uintptr_t>(amd64_exception_virtualization),
           GDT_KERN_CODE_SEG, Idt::kTrapFlags, Idt::Stack::Stack1);
}

/**
 * Handle a processor exception
 *
 * @param state Current processor state as built on the stack.
 */
void ExceptionHandlers::Handle(Processor::Regs &state) {
    using Type = Kernel::Exceptions::Handler::ExceptionType;

    /*
     * Handle the exception types that can be dispatched into the kernel's exception handling
     * machinery. These exceptions can be forwarded to tasks to handle.
     */
    switch(state.irq) {
        // Arithmetic exceptions
        case AMD64_EXC_DIVIDE:
            return Kernel::Exceptions::Handler::Dispatch(Type::DivideByZero, state);
        case AMD64_EXC_OVERFLOW:
        case AMD64_EXC_BOUNDS:
            return Kernel::Exceptions::Handler::Dispatch(Type::Overflow, state);
        case AMD64_EXC_FP:
            return Kernel::Exceptions::Handler::Dispatch(Type::FloatingPoint, state);
        case AMD64_EXC_SIMD_FP:
            return Kernel::Exceptions::Handler::Dispatch(Type::SIMD, state);
        // Opcode errors
        case AMD64_EXC_ILLEGAL_OPCODE:
            return Kernel::Exceptions::Handler::Dispatch(Type::InvalidOpcode, state);
        case AMD64_EXC_GPF:
            return Kernel::Exceptions::Handler::Dispatch(Type::ProtectionFault, state);
        // Memory errors
        case AMD64_EXC_ALIGNMENT:
            return Kernel::Exceptions::Handler::Dispatch(Type::AlignmentFault, state);
        // Debugging
        case AMD64_EXC_BREAKPOINT:
        case AMD64_EXC_DEBUG:
            return Kernel::Exceptions::Handler::Dispatch(Type::DebugBreakpoint, state);
    }

    /*
     * Handle other exceptions internally.
     *
     * These are used to affect context switching (disabling float/SIMD support until needed) and
     * also to handle machine errors.
     */
    switch(state.irq) {
        case AMD64_EXC_NMI:
            PANIC("Non-maskable interrupt");

        /*
         * Machine check errors indicate something is fucked in the system.
         *
         * There's better ways to handle this, including to get additional information, but we just
         * panic the system for now.
         */
        case AMD64_EXC_MCE:
            PANIC("Machine check (error $%016llx)", state.errorCode);

        /**
         * Double faults take place when we hit another fault during processing of the first fault,
         * which should really never happen.
         */
        case AMD64_EXC_DOUBLE_FAULT:
            // TODO: implement
            break;

        /**
         * Device unavailable exceptions are raised if we try to execute SIMD instructions, but the
         * SIMD units are disabled.
         *
         * We take advantage of this to know when a thread uses SIMD instructions, and save the
         * effort of saving/restoring these registers if it doesn't actually need them. Otherwise,
         * if the exception comes from kernel mode, we panic – the kernel shouldn't use SIMD.
         */
        case AMD64_EXC_DEVICE_UNAVAIL:
            // TODO: implement
            break;
    }

    // if we get here, the exception was unhandled
    constexpr static const size_t kStateBufSz{512};
    static char stateBuf[kStateBufSz];
    Platform::ProcessorState::Format(state, stateBuf, kStateBufSz);

    PANIC("Unhandled exception: %s\n%s", GetExceptionName(state.errorCode), stateBuf);
}

