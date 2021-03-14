#include "PerCpuInfo.h"

#include "idt.h"
#include "IrqRegistry.h"

#include <arch/x86_msr.h>
#include <log.h>

#include <new>

using namespace arch;

// memory for the BSP's per processor info struct
static char gBspInfoStruct[sizeof(ProcInfo)] __attribute__((aligned(64)));

/**
 * Allocates the per processor info for the bootstrap processor.
 *
 * We use the statically allocated bss memory for the object, then update the %gs base MSRs as we
 * would normally.
 */
void PerCpuInfo::BspInit() {
    // set up the info
    auto ptr = reinterpret_cast<ProcInfo *>(&gBspInfoStruct);
    auto proc = new(ptr) ProcInfo();

    // write the MSRs
    const auto procAddr = reinterpret_cast<uintptr_t>(proc);
    x86_msr_write(X86_MSR_GSBASE, procAddr, procAddr >> 32);
    x86_msr_write(X86_MSR_KERNEL_GSBASE, procAddr, procAddr >> 32);

    // install IDT
    proc->idt = gBspIdt;
    proc->irqRegistry = gBspIrqRegistry;
}

/**
 * Allocates per-processor info for the current processor, which is an application processor that
 * was started later on.
 */
void PerCpuInfo::ApInit() {
    // allocate the object
    auto proc = new ProcInfo;

    // TODO: get processor ID

    // set up some per-processor structures
    proc->idt = new Idt;
    proc->irqRegistry = new IrqRegistry(proc->idt);

    // store ptr
    const auto procAddr = reinterpret_cast<uintptr_t>(proc);
    x86_msr_write(X86_MSR_GSBASE, procAddr, procAddr >> 32);
    x86_msr_write(X86_MSR_KERNEL_GSBASE, procAddr, procAddr >> 32);
}
