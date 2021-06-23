#pragma once

#include "FAT.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

namespace fat {
class Directory;

class FAT32: public FAT {
    public:
        /// Allocates a new FAT32 instance.
        static int Alloc(const uint64_t start, const size_t length,
                const std::shared_ptr<DriverSupport::disk::Disk> &disk,
                const std::span<std::byte> &bpb, std::shared_ptr<Filesystem> &out);

        /// Return the root directory pointer
        std::shared_ptr<DirectoryBase> getRootDirectory() const override;

    protected:
        int readFat(const size_t n, std::vector<std::byte> &out) override;
        int getNextCluster(const uint32_t cluster, uint32_t &outNextCluster,
                bool &outIsLast) override;

    private:
        FAT32(const uint64_t start, const size_t length,
                const std::shared_ptr<DriverSupport::disk::Disk> &disk,
                const std::span<std::byte> &bpb);

    private:
        /// Whether adding/removing FAT sectors to the cache is logged
        constexpr static const bool kLogFatCache{false};
        /// Whether traversal of the FAT is logged
        constexpr static const bool kLogFatTraversal{false};

        /// if an error occurs during initialization it is stored here
        int status{0};

        /// FAT32 extended BPB
        ExtendedBpb32 bpb32;

        /**
         * Cache of FAT pages, indexed by the sector of the FAT.
         */
        std::unordered_map<uint32_t, std::vector<std::byte>> fatPageCache;

        /// Root directory
        std::shared_ptr<Directory> root;
};
}
