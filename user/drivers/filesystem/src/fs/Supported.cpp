#include "FilesystemRegistry.h"

#include "fat/FAT.h"
#include "test/SectorTest.h"

/**
 * Defines the list of FS matches that we can try against partition table entries.
 */
const std::array<FilesystemRegistry::Match, FilesystemRegistry::kNumFilesystems> 
FilesystemRegistry::gSupportedFs{
    /// FAT
    FilesystemRegistry::Match{
        {"File Allocation Table"},
        FAT::TryStart
    },

    /**
     * Randomly reads all the sectors on the partition and ensures they're read correctly. This
     * expects the contents of the partition to be a 4 byte integer value that constantly repeats
     * across the entire size.
     */
        FilesystemRegistry::Match{
        {"Sector Test"},
        SectorTestFs::TryStart
    },
};

