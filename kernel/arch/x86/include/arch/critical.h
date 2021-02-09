#ifndef ARCH_X86_CRITICAL_H
#define ARCH_X86_CRITICAL_H

#include "private.h"

#include <log.h>
#include <platform.h>

/**
 * Declares a critical section
 */
struct CriticalSection {
    platform::Irql lastIrql;

    /// enters the critical section
    inline void enter() {
        this->lastIrql = platform_get_irql();
        platform_raise_irql(platform::Irql::CriticalSection);
    }
    /// returns the irql to the previous level
    inline void exit() {
        platform_lower_irql(this->lastIrql);
    }
};

#define DECLARE_CRITICAL(name) CriticalSection __cs_name;
#define CRITICAL_ENTER(name) __cs_name.enter();
#define CRITICAL_EXIT(name) __cs_name.exit();

#endif
