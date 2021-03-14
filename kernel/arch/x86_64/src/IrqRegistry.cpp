#include "IrqRegistry.h"
#include "IrqStubs.h"

#include "idt.h"
#include "gdt.h"
#include "exceptions.h"
#include "exception_types.h"

#include "PerCpuInfo.h"

#include <new>
#include <log.h>
#include <string.h>

using namespace arch;

static uint8_t gSharedBuf[sizeof(IrqRegistry)] __attribute__((aligned(64)));
IrqRegistry *arch::gBspIrqRegistry = nullptr;

/**
 * Initializes the IRQ registry for the bootstrap processor.
 */
void IrqRegistry::Init() {
    gBspIrqRegistry = reinterpret_cast<IrqRegistry *>(&gSharedBuf);
    new(gBspIrqRegistry) IrqRegistry(gBspIdt);
}

/**
 * Sets up the IRQ registry. This will install IRQ handler stubs for all vectors in the allowable
 * IRQ range.
 */
IrqRegistry::IrqRegistry(Idt *_idt) : idt(_idt) {
    // clear registration info
    memset(this->registrations, 0, sizeof(HandlerRegistration) * kNumVectors);

    // set up IDT entries
    auto idt = arch::PerCpuInfo::get()->idt;

    for(size_t i = 0; i < kNumVectors; i++) {
        const auto vec = i + kVectorMin;
        idt->set(vec, (uintptr_t) kIrqStubTable[i], GDT_KERN_CODE_SEG, IDT_FLAGS_ISR,
                Idt::Stack::Stack6);
    }
}

/**
 * Handles an irq for the given vector number
 */
void IrqRegistry::handle(const uintptr_t vector) {
    // ensure it's in bounds
    REQUIRE(vector >= kVectorMin && vector <= kVectorMax, "invalid vector number: %u", vector);

    // find and execute handler if installed
    const auto &handler = this->registrations[vector - kVectorMin];
    if(handler.function) {
        handler.function(vector, handler.context);
    }
    // no installed handler
    else {
        log("got irq %3u, but no handlers installed!", vector);
    }
}



/**
 * IRQ handler stub: this calls through to the current processor's registry, which will then in
 * turn invoke the appropriate handler, if one is installed.
 */
void pc64_irq_entry(struct amd64_exception_info *info) {
    arch::PerCpuInfo::get()->irqRegistry->handle(info->errCode);
}
