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
    REQUIRE(vector >= kVectorMin && vector <= kVectorMax, "invalid vector number: %3u", vector);

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
 * Installs an irq handler.
 */
void IrqRegistry::install(const uintptr_t vector, Handler func, void *funcCtx, const bool replace) {
    // validate vector number
    REQUIRE(vector >= kVectorMin && vector <= kVectorMax, "invalid vector number: %3u", vector);
    const auto idx = vector - kVectorMin;
    auto &reg = this->registrations[idx];

    if(!replace) {
        REQUIRE(!reg.function, "refusing to replace vector %3u", vector);
    }

    // store it
    __atomic_store(&reg.context, &funcCtx, __ATOMIC_RELAXED);
    __atomic_store(&reg.function, &func, __ATOMIC_SEQ_CST);
}

/**
 * Removes a previously installed irq handler.
 */
void IrqRegistry::remove(const uintptr_t vector) {
    // validate vector number
    REQUIRE(vector >= kVectorMin && vector <= kVectorMax, "invalid vector number: %3u", vector);
    const auto idx = vector - kVectorMin;
    auto &reg = this->registrations[idx];

    REQUIRE(reg.function, "no handler for vector %3u, but removal requested", vector);

    // zero it out
    Handler nullHandler = reinterpret_cast<Handler>(NULL);
    void *nullPtr = nullptr;

    __atomic_store(&reg.function, &nullHandler, __ATOMIC_SEQ_CST);
    __atomic_store(&reg.context, &nullPtr, __ATOMIC_RELAXED);
}



/**
 * IRQ handler stub: this calls through to the current processor's registry, which will then in
 * turn invoke the appropriate handler, if one is installed.
 */
void pc64_irq_entry(struct amd64_exception_info *info) {
    arch::PerCpuInfo::get()->irqRegistry->handle(info->errCode);
}
