#pragma once

#include "FAT.h"

#include <cstddef>
#include <cstdint>
#include <span>

class FAT32: public FAT {
    public:
        /// Allocates a new FAT32 instance.
        static int Alloc(const uint64_t start, const size_t length,
                const std::shared_ptr<DriverSupport::disk::Disk> &disk,
                const std::span<std::byte> &bpb, std::shared_ptr<Filesystem> &out);

    private:
        FAT32(const uint64_t start, const size_t length,
                const std::shared_ptr<DriverSupport::disk::Disk> &disk,
                const std::span<std::byte> &bpb);

    private:
        /// if an error occurs during initialization it is stored here
        int status{0};

        /// FAT32 extended BPB
        ExtendedBpb32 bpb32;
};
