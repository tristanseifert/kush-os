#ifndef EXPERT_AMD64PC_H
#define EXPERT_AMD64PC_H

#include "Expert.h"

/**
 * Platform expert for amd64 PCs
 */
class Amd64PcExpert: public Expert {
    public:
        void probe() override;

    private:
        constexpr static const char *kAcpiServerPath = "/sbin/acpisrv";

    private:
        void exportFixed();

    private:
        /// Task handle to ACPI server
        uintptr_t acpiTaskHandle = 0;
};


#endif
