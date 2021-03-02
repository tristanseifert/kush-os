#include <thread>

#include "AcpicaWrapper.h"

#include "log.h"

#include <acpi.h>

/**
 * ACPI server entry point
 *
 * We are invoked by the appropriate platform expert in the driver manager during init; it may pass
 * to us one argument, which is a serialized info struct. (This is not currently used.)
 */
int main(const int argc, const char **argv) {
    // decode info struct if available
    if(argc >= 2) {
        // TODO: decode
        Trace("TODO: decode arg '%s'", argv[1]);
    }

    // initialise ACPICA
    AcpicaWrapper::init();

    // probe any busses and load drivers for built-in devices
    AcpicaWrapper::probeBusses();

    // enter main message loop
    Trace("Entering message loop");
    while(1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}
