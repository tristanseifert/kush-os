#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <span>

#include "Filesystem.h"

#include "partition/PartitionTable.h"

/**
 * Implements a simple File Allocation Table (FAT) driver. This supports read-only access to FAT32
 * volumes including support for long filenames (LFN).
 */
class FAT: public Filesystem {
    protected:
        /**
         * GUID for a Microsoft "Basic data partition" type. We have to read the first sector and
         * look at it to determine if it's actuall a FAT partition.
         */
        constexpr static const PartitionTable::Guid kBDPId{0xEB,0xD0,0xA0,0xA2,0xB9,0xE5,0x44,0x33,
            0x87,0xC0,0x68,0xB6,0xB7,0x26,0x99,0xC7};

        /**
         * Structure of the generic first sector of a FAT filesystem. This can be used to then
         * figure out what exact type of FAT we're dealing with.
         */
        struct Bpb {
            /// x86 jump opcode
            uint8_t  jmp[3];
            /// Identifier of the system that created this filesystem
            char oemId[8];

            uint16_t bytesPerSector;
            uint8_t sectorsPerCluster;
            /// Number of sectors between start of partition that are reserved
            uint16_t numReservedSectors;
            /// Number of copies of the FAT; typically 2
            uint8_t numFats;
            /// Number of root directory entries
            uint16_t numRootEntries;
            /// Number of logical sectors (if less than 2^16-1)
            uint16_t numSectors16;

            uint8_t mediaType;

            /// Number of sectors per FAT; valid only for FAT12/16
            uint16_t tableSize16;
            uint16_t sectorsPerTrack;
            uint16_t headSideCount;
            /// LBA of the start of the partition
            uint32_t numHiddenSectors;
            /// Number of logical sectors (if 2^16 or greater)
            uint32_t numSectors32;

            /// this area contains a type specific "extended" descriptor
            uint8_t extended[0];
        } __attribute__((packed));
        static_assert(sizeof(Bpb) == 0x24, "FAT BPB struct is invalid size");

        /**
         * Structure of the FAT12 and FAT16 extended boot information sector
         */
        struct ExtendedBpb16 {
            uint8_t biosDevice;
            uint8_t reserved;
            uint8_t bootSignature;
            /// Volume "serial number"
            uint32_t volumeId;
            /// User defined volume label, padded with spaces
            char volumeLabel[11];
            /// System identifier, also padded with spaces
            char fatTypeLabel[8];
        } __attribute__((packed));

        /**
         * Structure of the FAT32 extended boot information sector
         */
        struct ExtendedBpb32 {
            /// Number of sectors per FAT
            uint32_t tableSize32;
            uint16_t flags;
            /// FAT version; high byte is major, low byte is minor
            uint16_t fatVersion;
            /// Cluster containing the root directory
            uint32_t rootDirCluster;
            /// Sector containing the FSInfo structure
            uint16_t fsInfoSector;
            /// Sector containing the backup boot sector
            uint16_t backupBootSector;

            uint8_t reserved0[12];

            uint8_t biosDevice;
            uint8_t reserved1;
            uint8_t bootSignature;

            /// Volume "serial number"
            uint32_t volumeId;
            /// User defined volume label, padded with spaces
            char volumeLabel[11];
            /// System identifier, also padded with spaces
            char fatTypeLabel[8];
        } __attribute__((packed));

    public:
        /// FAT type
        enum class Type {
            Unknown, FAT12, FAT16, FAT32
        };

        /// FAT specific error codes
        enum Errors: int {
            /// The type of FAT is not supported
            UnsupportedFatType          = -66100,
        };

    public:
        FAT(const uint64_t start, const size_t length,
                const std::shared_ptr<DriverSupport::disk::Disk> &disk,
                const std::span<std::byte> &bpbData);

        /// Probe to see if we can attach to the partition, and start if so.
        static bool TryStart(const PartitionTable::Guid &id,
            const PartitionTable::Partition &partition,
            const std::shared_ptr<DriverSupport::disk::Disk> &disk,
            std::shared_ptr<Filesystem> &outFs, int &outErr);

    protected:
        /// Converts a cluster to a device absolute LBA
        virtual uint64_t clusterToLba(const uint32_t cluster);

        /// Determines whether the given sector contains a FAT12, 16, or 32 filesystem.
        static FAT::Type DetermineFatSize(const std::span<std::byte> &bpb);

    protected:
        /// First LBA belonging to this partition
        uint64_t startLba;
        /// Length of the FAT partition in sectors
        size_t numSectors;

        /// total number of clusters
        size_t numClusters;
        /// First sector (relative to START of partition) that may contain data
        size_t firstDataSector;

        /// Standard FAT BPB
        Bpb bpb;
};
