#include "Amd64PcExpert.h"
#include "Log.h"

#include <rpc/Task.h>

/**
 * Probes for devices.
 *
 * This loads drivers for some fixed hardware, then starts the ACPI server to discover
 * additional hardware.
 */
void Amd64PcExpert::probe() {
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

    Trace("ACPI task handle: $%p'h", this->acpiTaskHandle);

    // export fixed devices
    this->exportFixed();
}

/**
 * Exports fixed platform devices, i.e. those that are present on all 64 bit x86_64 PCs.
 */
void Amd64PcExpert::exportFixed() {

}
