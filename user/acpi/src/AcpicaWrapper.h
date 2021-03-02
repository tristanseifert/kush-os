#ifndef ACPISRV_ACPICAWRAPPER_H
#define ACPISRV_ACPICAWRAPPER_H

#include <cstdint>
#include <memory>
#include <unordered_map>

#include <acpi.h>

namespace acpi {
class Bus;
class PciBus;
}

/**
 * Provides a small wrapper around the ACPICA interfaces.
 */
class AcpicaWrapper {
    friend int main(const int, const char **);

    public:
        /// Initialize the global ACPICA wrapper
        static void init();
        /// Enumerate busses and initialize drivers for them
        static void probeBusses();

    private:
        static AcpicaWrapper *gShared;

        /// whether found busses are logged
        static bool gLogBusses;

    private:
        AcpicaWrapper();

        void installHandlers();
        void configureApic();

        void probePci();
        void foundPciRoot(ACPI_HANDLE);
        void pciGetIrqRoutes(ACPI_HANDLE, std::shared_ptr<acpi::PciBus> &);

    private:
        /// ID for the next bus we discover
        uintptr_t nextBusId = 1;
        /// all busses we've discovered
        std::unordered_map<uintptr_t, std::shared_ptr<acpi::Bus>> busses;
};

#endif
