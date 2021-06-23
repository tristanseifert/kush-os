#include "FilesystemRegistry.h"

/**
 * Defines the list of FS matches that we can try against partition table entries.
 */
const std::array<FilesystemRegistry::Match, FilesystemRegistry::kNumFilesystems> 
FilesystemRegistry::gSupportedFs = {
    /**
     * File Allocation Table: Supports currently only FAT32.
     */

};

