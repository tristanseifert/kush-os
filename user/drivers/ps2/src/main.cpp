#include <cstddef>
#include <vector>

#include <driver/DrivermanClient.h>

#include "Ps2Controller.h"
#include "Log.h"

const char *gLogTag = "ps2";
constexpr static const std::string_view kAuxDataKey{"ps2.resources"};

/**
 * Gets the PS/2 controller resource information from the forest.
 */
static void GetResourceInfo(const std::string_view &path, std::vector<std::byte> &outBytes) {
    auto rpc = libdriver::RpcClient::the();
    outBytes = rpc->GetDeviceProperty(path, kAuxDataKey);
}

/**
 * Entry point for the PS/2 controller: there can only ever be one of these in a system.
 *
 * The second argument is the driver forest path of this device.
 */
int main(const int argc, const char **argv) {
    // retrieve the aux data from the RPC properties
    std::vector<std::byte> auxData;

    if(argc != 2) {
        Abort("must have exactly one argument");
    }

    GetResourceInfo(argv[1], auxData);
    if(auxData.empty()) {
        Abort("Failed to get aux data (%s %s)", argv[1], kAuxDataKey);
    }

    // enter controller work loop
    Ps2Controller c(auxData);
    c.workerMain();
}
