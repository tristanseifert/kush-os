#include "LocalApic.h"

#include <platform.h>

#include <arch/PerCpuInfo.h>
#include <log.h>

using namespace platform;

static bool gLogIrql = false;

/**
 * Raises the interrupt priority level of the current processor. The previous irql is returned.
 */
platform::Irql platform_raise_irql(const platform::Irql irql, const bool enableIrq) {
    Irql prev, newVal = irql;
    auto info = arch::PerCpuInfo::get();

    asm volatile("cli");
    REQUIRE(irql >= info->irql, "cannot %s irql: current %d, requested %d", "raise",
            (int) info->irql, (int) irql);
    __atomic_exchange(&info->irql, &newVal, &prev, __ATOMIC_ACQUIRE);

    if(gLogIrql) log("raise irql: %u (%p)", (uintptr_t) irql, info->p.lapic);
    if(info->p.lapic) {
        info->p.lapic->updateTpr(irql);
    }

    if(enableIrq) {
        asm volatile("sti");
    }

    return prev;


    panic("%s unimplemented", __PRETTY_FUNCTION__);
}

/**
 * Lowers the interrupt priority level of the current processor.
 */
void platform_lower_irql(const platform::Irql irql, const bool enableIrq) {
    auto info = arch::PerCpuInfo::get();

    asm volatile("cli");
    REQUIRE(irql <= info->irql, "cannot %s irql: current %d, requested %d", "lower", 
            (int) info->irql, (int) irql);

    Irql _irql = irql;
    __atomic_store(&info->irql, &_irql, __ATOMIC_RELAXED);

   if(gLogIrql) log("lower irql: %u (%p)", (uintptr_t) irql, info->p.lapic);
    if(info->p.lapic) {
        info->p.lapic->updateTpr(irql);
    }

    if(enableIrq) {
        asm volatile("sti");
    }
}

/**
 * Gets the current irql of the processor.
 */
const platform::Irql platform_get_irql() {
    return arch::PerCpuInfo::get()->irql;
}
