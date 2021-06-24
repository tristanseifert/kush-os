#pragma once

#include "Directory.h"
#include "fs/File.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fat {
/**
 * Represents a file as read from the FAT filesystem.
 *
 * This is really just a thin wrapper that caches the cluster chain of the file and emulates random
 * access with byte granularity over it with full cluster reads.
 */
class File: public ::FileBase {
    public:
        /// Create a new file from the given FAT directory entry.
        static int Alloc(DirectoryEntry * _Nonnull dent, FAT * _Nonnull fs,
                std::shared_ptr<File> &outFile);

        /// Create a file with the given directory entry.
        File(DirectoryEntry * _Nonnull dent, FAT * _Nonnull fs);

        /// Get the filename
        const std::string &getName() const override {
            return this->name;
        }
        /// Get the full path (relative to the root of this fs) of the file
        const std::string &getFullPath() const override;
        /// Get the size of the file in bytes
        uint64_t getFileSize() const override {
            return this->fileSize;
        }

        /// Perform file IO; this resolves the cluster chain as needed
        int read(const uint64_t offset, const size_t numBytes,
                std::vector<std::byte> &readBuf) override;

    private:
        /// Filesystem from which the file was read
        FAT * _Nonnull fs;

        /// Size of file, in bytes
        uint32_t fileSize{0};
        /// Address of the file's first cluster
        uint32_t firstCluster{0};

        /// Name of the file
        std::string name;

        /**
         * Cache of the cluster chain of the file. This array is indexed by the number of the
         * cluster relative from the start of the file, and in that slot contains the on disk
         * address of the subsequent cluaster of data, or an all 1's value to indicate that this
         * is the end of the file.
         */
        std::vector<uint32_t> clusterMap;
};
}
