#include <cstddef>
#include <vector>

#include <driver/DrivermanClient.h>
#include <DriverSupport/disk/Client.h>

#include "FilesystemRegistry.h"
#include "auto/Automount.h"
#include "partition/GPT.h"
#include "rpc/MessageLoop.h"
#include "Log.h"

const char *gLogTag = "fs";

/**
 * Entry point for the filesystem server, attached to a disk. The arguments to the function are
 * paths to disks to attach to.
 */
int main(const int argc, const char **argv) {
    int err;
    if(argc < 2) {
        Abort("You must specify at least one forest path of a disk.");
    }

    // perform initialization
    FilesystemRegistry::Init();
    Automount::Init();

    MessageLoop ml;

    // for each argument, create a disk and probe the partition table
    for(size_t i = 1; i < argc; i++) {
        const auto path = argv[i];

        // create disk object
        std::shared_ptr<DriverSupport::disk::Disk> disk;
        err = DriverSupport::disk::Disk::Alloc(path, disk);
        if(err) {
            Warn("Failed to allocate disk from '%s': %d", path, err);
            continue;
        }

        Success("Opened drive: %s", disk->getForestPath().c_str());

        // probe to see the partition table of this disk
        std::shared_ptr<PartitionTable> tab;

        err = GPT::Probe(disk, tab);
        if(err) {
            Warn("Failed to detect GPT on '%s': %d", path, err);
            continue;
        }

        // read the partition tables and try to initialize a filesystem for each
        const auto &tabs = tab->getPartitions();
        Success("Got %lu partitions", tabs.size());

        for(const auto &p : tabs) {
            // try to create FS
            std::shared_ptr<Filesystem> fs;
            err = FilesystemRegistry::the()->start(p.typeId, p, disk, fs);

            if(!err) {
                // add to automounter
                Automount::the()->startedFs(disk, p, fs);
            } else {
                const auto &i = p.typeId;
                Trace("Failed to initialize fs (%d) %10lu (%10lu sectors): %02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X - %s", err,
                        p.startLba, p.size, i[0],i[1],i[2],i[3],i[4],i[5],i[6],i[7],i[8],i[9],i[10],
                        i[11],i[12],i[13],i[14],i[15], p.name.value_or("(no name)").c_str());
            }

        }
    }

    // perform post-mount notifications
    Automount::the()->postMount();

    // start RPC server and enter the message loop
    err = ml.run();
    Warn("Message loop returned: %d", err);

    FilesystemRegistry::Deinit();
    return 0;
}
