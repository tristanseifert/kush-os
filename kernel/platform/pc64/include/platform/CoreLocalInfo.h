#ifndef PLATFORM_PC64_CORELOCALINFO_H
#define PLATFORM_PC64_CORELOCALINFO_H

#ifndef ASM_FILE

namespace platform {
class LocalApic;
class Tsc;

/**
 * PC platform specific information that's stored in the core local info block.
 */
struct CoreLocalInfo {
    /// Core local APIC
    LocalApic *lapic = nullptr;
    /// TSC timer wrapper
    Tsc *tsc = nullptr;
};
}

#endif

#define PLATFORM_WANTS_CORE_LOCAL 1
#endif
