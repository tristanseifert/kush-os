#ifndef EXPERT_X86PC_H
#define EXPERT_X86PC_H

#include "Expert.h"

/**
 * Platform expert for x86 PCs
 */
class X86PcExpert: public Expert {
    public:
        void probe() override;

    private:
        constexpr static const char *kAcpiServerPath = "/sbin/acpisrv";

    private:
        /// Task handle to ACPI server
        uintptr_t acpiTaskHandle = 0;
};

#endif
