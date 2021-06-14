#include "Amd64PcExpert.h"
#include "Log.h"

#include <rpc/Task.h>

#include "forest/Device.h"
#include "forest/DriverInstance.h"
#include "forest/Forest.h"

/**
 * Probes for devices.
 *
 * In our case, this just creates the ACPI root device that all dynamically discovered devices
 * will attach under. This automatically loads the ACPI server, so no further action is required.
 */
void Amd64PcExpert::probe() {
    std::string rootPath;

    // create the ACPI device
    auto driver = std::make_shared<Device>(kAcpiServerDriverName);
    Forest::the()->insertDevice("/", driver, rootPath);
}

