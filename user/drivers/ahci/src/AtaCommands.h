#ifndef AHCIDRV_ATACOMMANDS_H
#define AHCIDRV_ATACOMMANDS_H

#include <cstdint>

/**
 * Meaning of bits in the 8-bit ATA status register.
 *
 * Note that the contents of the register are meaningless while `BSY` is set.
 */
enum AtaStatus: uint8_t {
    /// An error occurred
    Error                               = (1U << 0),
    /// Device is ready; clear if device is spun down, or after an error
    Ready                               = (1U << 6),
    /// The device is busy. Do not consider other bits as long as this is set.
    Busy                                = (1U << 7),
};


/**
 * Enumeration of possible ATA commands.
 */
enum class AtaCommand: uint8_t {
    /**
     * 0xEC: IDENTIFY DEVICE
     *
     * Returns a 512-byte block of information to the host about this device. If the device is an
     * ATA packet device ((S)ATAPI) this command will fail; you should use the IDENTIFY PACKET
     * DEVICE command instead.
     */
    Identify                            = 0xEC,
    /**
     * 0xA1: IDENTIFY PACKET DEVICE
     *
     * Returns a 512-byte block of information to the host about this device. It is functionally
     * similar to IDENTIFY DEVICE command.
     */
    IdentifyPacket                      = 0xA1,
};

#endif
