#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <optional>
#include <vector>

/**
 * Abstract interface of a partition table on a disk.
 */
class PartitionTable {
    public:
        /// Defines a 128-bit GUID stored as a sequence of 16 bytes.
        using Guid = std::array<uint8_t, 16>;

        /**
         * Describes information for a single partition. Regardless of the actual underlying IDs
         * used by the partition table, they are converted to GPT-style UUIDs for the rest of the
         * system to use.
         */
        struct Partition {
            /// Partition index
            size_t index;
            /// Partition type (UUID)
            Guid typeId;
            /// First sector of the partition
            uint64_t startLba;
            /// Length of the partition, in sectors
            uint64_t size;

            /// Partition unique id, if the partitioning table format supports it
            std::optional<Guid> partitionGuid;
            /// Display name, if the partitioning table format supports it
            std::optional<std::string> name;
        };

    public:
        virtual ~PartitionTable() = default;

        /// Descriptive name of the partition table format
        virtual const std::string_view &getFormatName() const = 0;
        /// Returns information on all partitions in the partition table
        virtual const std::vector<Partition> &getPartitions() const = 0;
};
