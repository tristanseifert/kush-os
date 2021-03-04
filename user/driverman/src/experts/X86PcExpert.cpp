#include "X86PcExpert.h"
#include "Log.h"

#include <rpc/task.h>

/**
 * Probes for devices.
 *
 * This loads drivers for some fixed hardware, then starts the ACPI server to discover
 * additional hardware.
 */
void X86PcExpert::probe() {
    // create some architectural shit

    // start ACPI server
    const char *args[] = {
        kAcpiServerPath,
        nullptr
    };

    int err = RpcTaskCreate(kAcpiServerPath, args, &this->acpiTaskHandle);
    if(err) {
        Abort("failed to launch ACPI server: %d", err);
    }

    Trace("ACPI task handle: $%08x'h", this->acpiTaskHandle);

    // export fixed devices
    this->exportFixed();
}

/**
 * Exports fixed platform devices, i.e. those that are present on all x86 PCs.
 */
void X86PcExpert::exportFixed() {

}
