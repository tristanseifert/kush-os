#include <cstddef>
#include <vector>

#include <iomanip>
#include <sstream>

#include <driver/DrivermanClient.h>
#include <DriverSupport/disk/Client.h>

#include "Log.h"

const char *gLogTag = "fs";

/**
 * Prints a sector of 512 bytes.
 */
static void PrintSector(const std::vector<std::byte> &data) {
    std::stringstream str;

    for(size_t i = 0; i < data.size(); i++) {
        if(!(i % 32)) {
            str << std::setw(4) << std::hex << i << ": ";
        }

        str << std::hex << static_cast<uint32_t>(data[i]) << " ";

        if(!(i % 32) && i) {
            str << std::endl;
        }
    }

    Trace("Sector is:\n%s", str.str().c_str());
}

/**
 * Entry point for the filesystem server, attached to a disk. The arguments to the function are
 * paths to disks to attach to.
 */
int main(const int argc, const char **argv) {
    if(argc < 2) {
        Abort("must have exactly one argument");
    }

    std::shared_ptr<DriverSupport::disk::Disk> outDisk;
    int err = DriverSupport::disk::Disk::Alloc(argv[1], outDisk);
    if(err) Abort("Failed to allocate disk from '%s': %d", argv[1], err);

    // size disk
    std::pair<uint32_t, uint64_t> cap;
    err = outDisk->GetCapacity(cap);
    if(err) Abort("Failed to size disk: %d", err);

    Success("Disk size is %lu bytes (%lu sectors x %u bytes)", cap.first * cap.second, cap.second, cap.first);

    // read some data
    std::vector<std::byte> data;
    err = outDisk->Read(0, 1, data);
    if(err) Abort("ReadSlow failed: %d", err);

    Success("Read %lu bytes", data.size());
    PrintSector(data);

    return 1;
}
