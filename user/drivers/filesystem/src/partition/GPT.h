#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <string_view>

#include <DriverSupport/disk/Client.h>

#include "PartitionTable.h"

/**
 * Represents a GPT formatted partition table, as read from the disk.
 */
class GPT: public PartitionTable {
    /// LBA at which the GPT header is located
    constexpr static const uint64_t kHeaderLba{1};
    /// Name for the format
    constexpr static const std::string_view kFormatName{"GPT"};

    private:
        /**
         * EFI GUID representation; this is some bullshit "mixed endian" nonsense, so the fields
         * inside this struct need to be taken care of specifically when writing them out. It's
         * kind of a pain in the ass so there's conversion operators.
         */
        struct GptGuid {
            uint32_t timeLow;
            uint16_t timeMid;
            uint16_t timeHighVers;
            uint8_t  clockSeqHigh;
            uint8_t  clockSeqLow;
            uint8_t  node[6];

            /**
             * Converts this mess into a proper linearly laid out UUID buffer.
             */
            inline operator std::array<uint8_t, 16>() const {
                std::array<uint8_t, 16> arr;
                arr[0] = timeLow >> 24;
                arr[1] = timeLow >> 16;
                arr[2] = timeLow >>  8;
                arr[3] = timeLow & 0xFF;
                arr[4] = timeMid >>  8;
                arr[5] = timeMid & 0xFF;
                arr[6] = timeHighVers >> 8;
                arr[7] = timeHighVers & 0xFF;
                arr[8] = clockSeqHigh;
                arr[9] = clockSeqLow;
                std::copy(&node[0], &node[6], arr.begin()+10);
                return arr;
            }

            /// Whether this is a nil (all zeros) GUID
            constexpr inline bool isNil() const {
                return !timeLow && !timeMid && !timeHighVers && !clockSeqHigh && !clockSeqLow
                    && !node[0] && !node[1] && !node[2] && !node[3] && !node[4] && !node[5];
            }
        } __attribute__((packed));

        /**
         * Structure of the header of the GPT. All checksums are CRC32's that are computed with
         * the standard Ethernet (0x04c11db7) CRC polynomials.
         */
        struct GptHeader {
            /// Magic value to detect in the first 8 bytes of the header LBA
            constexpr static const std::string_view kMagic{"EFI PART"};
            /// Minimum required specification version
            constexpr static const uint32_t kMinRevision{0x00010000};

            /// Magic value
            uint8_t magic[8];

            /// GPT revision; currently 0x00010000
            uint32_t revision;
            /// Size of the header, in bytes
            uint32_t headerSize;
            /// CRC32 of the header
            uint32_t headerChecksum;
            uint32_t reserved0;

            /// LBA containing the GPT header
            uint64_t headerLba;
            /// LBA containing the alternate GPT header
            uint64_t backupHeaderLba;
            /// First LBA usable for user data
            uint64_t firstUsableLba;
            /// Last LBA usable for user data
            uint64_t lastUsableLba;

            /// Disk UUID
            GptGuid diskId;

            /// Starting LBA of the actual partition list
            uint64_t partitionTableLba;
            /// Number of total partition entries
            uint32_t numPartitionEntries;
            /// Size of each partition entry (must be multiple of 8)
            uint32_t partitionEntrySize;
            /// Checksum over the entire partition table entry array
            uint32_t partitionEntryChecksum;
        } __attribute__((packed));

        /**
         * Structure of a single entry in the partition table. Note that the size of this struct
         * on disk may be larger than this definition, in which case any extra space after the
         * end is simply ignored.
         */
        struct GptPartition {
            /// Partition type GUID; all zeros indicates unused
            GptGuid partitionTypeGuid;
            /// Partition unique GUID (if valid)
            GptGuid partitionUniqueGuid;

            /// LBA of the start of this partition
            uint64_t lbaStart;
            /// LBA of the end of this partition
            uint64_t lbaEnd;

            /// Attribute bits; currently ignored
            uint64_t attributes;

            /// Unicode (UCS-2) string
            uint16_t name[36];
        };

    public:
        enum Errors: int {
            /// The magic value of the partition table didn't match.
            InvalidMagic                        = -65000,
            /// The header is invalid
            InvalidHeader                       = -65001,
            /// Checksum in the GPT header is invalid
            HeaderChecksumMismatch              = -65002,
            /// GPT is unsupported version
            UnsupportedVersion                  = -65003,
            /// Partition table checksum did not match
            TableChecksumMismatch               = -65004,
            /// The size of the partition tables is invalid.
            InvalidTableSize                    = -65005,
        };

    public:
        static int Probe(const std::shared_ptr<DriverSupport::disk::Disk> &disk,
                std::shared_ptr<PartitionTable> &outTable);

        /// Return the name of this partitioning scheme
        const std::string_view &getFormatName() const {
            return kFormatName;
        }
        /// Returns the partitions we've loaded.
        const std::vector<Partition> &getPartitions() const {
            return this->partitions;
        }

    private:
        GPT(const std::shared_ptr<DriverSupport::disk::Disk> &disk,
                const std::span<std::byte> &header);

        int readPartitionTable(const std::shared_ptr<DriverSupport::disk::Disk> &,
                const GptHeader &);

        static std::string ConvertUcs2ToUtf8(const std::span<uint16_t> &ucs2Str);

    private:
        /// Initialization status; nonzero indicates error
        int status{0};

        /// Read partition objects
        std::vector<Partition> partitions;

        /// Disk GUID
        std::array<uint8_t, 16> diskId;
        /// Range of usable LBAs
        std::pair<uint64_t, uint64_t> usableLbas;
};
