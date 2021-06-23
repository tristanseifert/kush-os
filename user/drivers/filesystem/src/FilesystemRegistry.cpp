#include "FilesystemRegistry.h"

#include "Log.h"

FilesystemRegistry *FilesystemRegistry::gShared{nullptr};

/**
 * Initializes the file system registry.
 */
void FilesystemRegistry::Init() {
    if(gShared) Abort("Cannot reinitialize FS registry");
    gShared = new FilesystemRegistry;
}

/**
 * Shut the global filesystem registry down.
 */
void FilesystemRegistry::Deinit() {
    if(!gShared) Abort("Cannot deinit an uninitialized FS registry");
    delete gShared;
    gShared = nullptr;
}

/**
 * Tries to instantiate a filesystem for the given type GUID on the given partition. If a
 * filesystem was attached to the partition, it's automatically registered.
 *
 * @return 0 on success, or an error code indicating why we couldn't create a filesystem.
 */
int FilesystemRegistry::start(const PartitionTable::Guid &guid,
        const PartitionTable::Partition &part,
        const std::shared_ptr<DriverSupport::disk::Disk> &disk,
        std::shared_ptr<Filesystem> &outFs) {
    // test each fs
    for(const auto &match : gSupportedFs) {
        int err{0};
        if(match.probe(guid, part, disk, outFs, err)) {
            return err;
        }
    }

    // if we get here, failed to find one that matches
    return Errors::UnknownFs;
}

