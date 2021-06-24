#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <span>
#include <vector>

#include "fs/Filesystem.h"

#include "partition/PartitionTable.h"

namespace fat {
class DirectoryEntry;
class Directory;
class File;
}

/**
 * Implements a simple File Allocation Table (FAT) driver. This supports read-only access to FAT32
 * volumes including support for long filenames (LFN).
 */
class FAT: public Filesystem {
    friend class fat::DirectoryEntry;
    friend class fat::File;

    /**
     * Information associated with a long file name; this is information we build up as we read
     * multiple LFN entries.
     */
    struct LfnInfo {
        /**
         * Total number of LFN entries expected; this is set when we encounter the LFN entry that
         * has the `lastLfn` bit set.
         */
        uint8_t totalEntries;
        /// Checksum value of the expected short name
        uint8_t shortNameChecksum;

        /// Number of LFN entries we've read so far
        uint8_t readEntries{0};


        /**
         * Buffer into which all characters of the long filename are read. These are UCS-2 chars,
         * and are converted to UTF-8 before we trim any excess spaces at the end.
         */
        std::vector<uint16_t> charBuf;
    };
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

        /**
         * Format of a directory entry on disk. This is the primary "old style" 8.3 name format,
         * while for LFN support, an alternate format exists.
         */
        struct DirEnt {
            /// Values for directory entry attributes
            enum Attributes: uint8_t {
                ReadOnly                = (1 << 0),
                Hidden                  = (1 << 1),
                System                  = (1 << 2),
                VolumeId                = (1 << 3),
                Directory               = (1 << 4),
                Archive                 = (1 << 5),

                /// Indicates this is a long file name continuation
                LongFileName            = ReadOnly|Hidden|System|VolumeId,

                /// Mask for the contents of the attribute byte; the top 2 bits are reserved
                Mask                    = 0b00111111,
            };

            /// Name component of filename
            unsigned char name[8];
            /// Extension component of filename
            unsigned char extension[3];
            /// File attributes
            Attributes attributes;
            uint8_t reserved;

            /// Creation time in tenths of seconds
            uint8_t creationSecondTenths;

            uint16_t creationSeconds    : 5;
            uint16_t creationMinutes    : 6;
            uint16_t creationHours      : 5;
            uint16_t creationDay        : 5;
            uint16_t creationMonth      : 4;
            uint16_t creationYear       : 7;

            uint16_t accessDay          : 5;
            uint16_t accessMonth        : 4;
            uint16_t accessYear         : 7;

            /// High 16 bits of the first cluster of this file
            uint16_t clusterHigh;

            uint16_t modificationSeconds: 5;
            uint16_t modificationMinutes: 6;
            uint16_t modificationHours  : 5;
            uint16_t modificationDay    : 5;
            uint16_t modificationMonth  : 4;
            uint16_t modificationYear   : 7;

            /// Low 16 bits of the first cluster of this file
            uint16_t clusterLow;
            /// File size, in bytes
            uint32_t fileSize;

            constexpr uint32_t getCluster() const {
                return this->clusterLow | (this->clusterHigh << 16U);
            }
        } __attribute__((packed));
        static_assert(sizeof(DirEnt) == 32, "Invalid size for FAT directory entry");

        /**
         * A long file name directory entry; this is identified by the attribute field being set to
         * the LFN attribute value.
         */
        struct LfnDirEnt {
            /// Order of this LFN entry
            uint8_t order:6;
            uint8_t lastLfn:1;
            uint8_t reserved0:1;
            /// First 5 characters of this long filename
            uint16_t chars1[5];

            /// File attribute value; should be Attribute::LongFileName
            DirEnt::Attributes attribute;
            uint8_t reserved1;

            /// checksum over the corresponding file's short filename
            uint8_t shortNameChecksum;

            /// Next six characters of filename
            uint16_t chars2[6];
            uint16_t reserved2;
            /// Last 2 characters of this filename
            uint16_t chars3[2];
        } __attribute__((packed));
        static_assert(sizeof(LfnDirEnt) == sizeof(DirEnt),
                "Invalid size for long file name directory entry");

    public:
        /// FAT type
        enum class Type {
            Unknown, FAT12, FAT16, FAT32
        };

        /// FAT specific error codes
        enum Errors: int {
            /// The type of FAT is not supported
            UnsupportedFatType          = -66200,
            /// FAT sector number is out of range
            FatSectorOutOfRange         = -66201,
            /// Provided directory entry is invalid
            InvalidDirectoryEntry       = -66202,
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

        /// Return volume label; this is read when we load the root directory
        std::optional<std::string> getVolumeLabel() const override {
            return this->volumeLabel;
        }

        int readDirectory(DirectoryEntryBase *dent, std::shared_ptr<DirectoryBase> &out) override;
        int openFile(DirectoryEntryBase *dent, std::shared_ptr<FileBase> &out) override;

    protected:
        /// Converts a cluster to a device absolute LBA
        virtual uint64_t clusterToLba(const uint32_t cluster);
        /// Reads a particular cluster.
        virtual int readCluster(const uint32_t cluster, std::vector<std::byte> &outData,
                const size_t numSectors = 0);
        /// Reads the directory starting with the given cluster.
        virtual int readDirectory(const uint32_t cluster, std::shared_ptr<fat::Directory> &outDir,
                const bool isRoot);

        /**
         * Reads the `n`th sector of the FAT into the provided buffer.
         *
         * @return 0 on success or an error code.
         */
        virtual int readFat(const size_t n, std::vector<std::byte> &out) = 0;

        /**
         * Reads the FAT to determine what the next cluster following the one provided is, if there
         * is one.
         *
         * @param cluster Current cluster value to check for the next cluster
         * @param outNextCluster Next cluster, if any
         * @parma outIsLast Set if this was the last cluster in the chain
         *
         * @return 0 on success or an error code.
         */
        virtual int getNextCluster(const uint32_t cluster, uint32_t &outNextCluster,
                bool &outIsLast) = 0;

        /// Determines whether the given sector contains a FAT12, 16, or 32 filesystem.
        static FAT::Type DetermineFatSize(const std::span<std::byte> &bpb);
        /// Calculates the checksum of a short filename used by LFN dir entries
        static uint8_t CalculateShortNameChecksum(const DirEnt &ent);

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

        /// Optional volume label
        std::optional<std::string> volumeLabel;
};
