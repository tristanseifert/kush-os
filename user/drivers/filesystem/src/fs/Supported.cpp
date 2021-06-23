#include "FilesystemRegistry.h"

#include "fat/FAT.h"

/**
 * Defines the list of FS matches that we can try against partition table entries.
 */
const std::array<FilesystemRegistry::Match, FilesystemRegistry::kNumFilesystems> 
FilesystemRegistry::gSupportedFs{
    /// FAT
    FilesystemRegistry::Match{
        {"File Allocation Table"},
        FAT::TryStart
    }
};

