#ifndef ACPISRV_ACPICAWRAPPER_H
#define ACPISRV_ACPICAWRAPPER_H

#include <cstdint>

#include <acpi.h>

/**
 * Provides a small wrapper around the ACPICA interfaces.
 */
class AcpicaWrapper {
    friend int main(const int, const char **);

    public:
        /// Initialize the global ACPICA wrapper
        static void init();
        /// Enumerate busses and initialize drivers for them
        static void probeBusses() {
            gShared->probePci();
        }

    private:
        static AcpicaWrapper *gShared;

    private:
        AcpicaWrapper();

        void installHandlers();
        void configureApic();

        void probePci();
        void foundPciRoot(ACPI_HANDLE);
        void pciGetIrqRoutes(ACPI_HANDLE);
};

#endif
