#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>

#include "fs/Filesystem.h"
#include "partition/PartitionTable.h"

class FilesystemRegistry {
    /**
     * Method invoked to probe a filesystem.
     *
     * @param GUID of the filesystem (to identify the type)
     * @param Partition on which the filesystem resides
     * @param Output pointer for the allocated filesystem
     * @param If an error occurs during initalization (once the type is matched via GUID) it shall
     *        be written in this field.
     * @return Whether the filesystem was attempted to be started.
     */
    using ProbeFxn = std::function<bool(const PartitionTable::Guid &,
            const PartitionTable::Partition &, const std::shared_ptr<DriverSupport::disk::Disk> &,
            std::shared_ptr<Filesystem> &, int &)>;

    public:
        enum Errors: int {
            /// No filesystem knows how to handle this partition type
            UnknownFs                   = -66000,
        };

    public:
        /// Initialize the global FS registry.
        static void Init();
        /// Deallocate the FS registry.
        static void Deinit();
        /// Returns the global FS registry instance.
        static FilesystemRegistry *the() {
            return gShared;
        }

        /// Attempt to load a filesystem with the given identifier and partition info
        int start(const PartitionTable::Guid &guid, const PartitionTable::Partition &part,
                const std::shared_ptr<DriverSupport::disk::Disk> &disk,
                std::shared_ptr<Filesystem> &outFs);

    private:
        /// Info on a single filesystem that may match
        struct Match {
            /// Description of the match descriptor
            const std::string name;
            /// Invoke to probe the filesystem against the device
            ProbeFxn probe;
        };

    private:
        static FilesystemRegistry *gShared;

        /// Number of installed filesystems
        constexpr static const size_t kNumFilesystems{2};
        /// List of all supported filesystems
        static const std::array<Match, kNumFilesystems> gSupportedFs;
};
