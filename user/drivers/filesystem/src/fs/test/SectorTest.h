#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <thread>
#include <span>
#include <vector>

#include "fs/Filesystem.h"

#include "partition/PartitionTable.h"

class SectorTestFs : public Filesystem {
    /**
     * GUID of the partition type
     */
    constexpr static const PartitionTable::Guid kTypeId{0xEC, 0xEE, 0xD8, 0x56, 0x9B, 0x5B, 0x46,
        0x06, 0xA5, 0x7C, 0xBA, 0x1F, 0x3B, 0x96, 0x61, 0xCB};

    public:
        /// FS specific error codes
        enum Errors: int {
            /// Requested operation isn't supported
            Unsupported                 = -66300,
        };

    public:
        ~SectorTestFs();

        /// Probe to see if we can attach to the partition, and start if so.
        static bool TryStart(const PartitionTable::Guid &id,
            const PartitionTable::Partition &partition,
            const std::shared_ptr<DriverSupport::disk::Disk> &disk,
            std::shared_ptr<Filesystem> &outFs, int &outErr);
        /// Allocate a new instance of the sector testing fs
        static int Alloc(const uint64_t start, const size_t length,
                const std::shared_ptr<DriverSupport::disk::Disk> &disk,
                std::shared_ptr<Filesystem> &out);

        // we don't actually support IO
        std::shared_ptr<DirectoryBase> getRootDirectory() const override {
            return nullptr;
        }
        int readDirectory(DirectoryEntryBase *dent, std::shared_ptr<DirectoryBase> &out) override {
            return Errors::Unsupported;
        }
        int openFile(DirectoryEntryBase *dent, std::shared_ptr<FileBase> &out) override {
            return Errors::Unsupported;
        }

    private:
        SectorTestFs(const uint64_t start, const size_t length,
                const std::shared_ptr<DriverSupport::disk::Disk> &disk);

        void workerMain();

    private:
        // used to indicate failures during initialization
        int status{0};

        /// First LBA belonging to this partition
        uint64_t startLba;
        /// Length of the FAT partition in sectors
        size_t numSectors;

        /// worker boi
        std::unique_ptr<std::thread> worker;
        /// run the worker boi
        std::atomic_bool run{true};
};
