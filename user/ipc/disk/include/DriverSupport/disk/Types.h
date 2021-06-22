#ifndef DRIVERSUPPORT_DISK_TYPES_H
#define DRIVERSUPPORT_DISK_TYPES_H

#include <cstddef>
#include <cstdint>

namespace DriverSupport::disk {
/**
 * Defines a command type.
 */
enum class CommandType: uint8_t {
    None,
    Read,
    Write,
};

/**
 * Structure of a single command in the command list that's stored in the disk command shared
 * memory region.
 */
struct Command {
    /// set to mark the command as allocated
    bool allocated{false};
    /// whether the command is busy (being executed or is queued)
    bool busy{false};
    /// whether the command has completed
    bool completed{false};
    /// type of command
    CommandType type{CommandType::None};

    /// completion status code for the command; 0 = success
    int __attribute((aligned(4))) status{0};

    /// Thread to notify when command completes
    uintptr_t notifyThread;
    /// Notification bits to set when command completes
    uintptr_t notifyBits;

    /// disk id to access
    uint64_t diskId{0};

    /// Starting sector for command
    uint64_t sector;
    /// Offset into read/write buffer
    uint64_t bufferOffset;

    /// Total number of sectors to read/write
    uint32_t numSectors;
    /// Total bytes that were actually transfered
    uint32_t bytesTransfered;

    uint8_t reserved[8];
} __attribute__((packed));

static_assert(sizeof(Command) == 0x40, "Invalid size for command descriptor");
}

#endif
