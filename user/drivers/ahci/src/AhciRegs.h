#ifndef AHCIDRV_AHCIREGS_H
#define AHCIDRV_AHCIREGS_H

#include <cstddef>
#include <cstdint>

/**
 * Known device signature values
 */
enum AhciDeviceSignature: uint32_t {
    /// Plain SATA device
    SATA                                = 0x00000101,
    /// SATAPI device
    SATAPI                              = 0xEB140101,
    /// Enclosure management bridge
    EnclosureManagement                 = 0xC33C0101,
    /// SATA port multiplier
    PortMultiplier                      = 0x96690101,
};

/**
 * Structure representing the specific register layout of a single AHCI port.
 */
struct AhciHbaPortRegisters {
    // 0x00, command list base address, 1K-byte aligned
    uint32_t cmdListBaseLow;
    // 0x04, command list base address, high 32 bits
    uint32_t cmdListBaseHigh;
    // 0x08, FIS base address, 256-byte aligned
    uint32_t fisBaseLow;
    // 0x0C, FIS base address, high 32 bits
    uint32_t fisBaseHigh;

    // 0x10, Interrupt status
    uint32_t irqStatus;
    // 0x14, Interrupt enable
    uint32_t irqEnable;
    // 0x18, Command and status
    uint32_t command;
    uint32_t reserved0;

    // 0x20, Task file data
    uint32_t taskFileData;
    // 0x24, Device signature
    uint32_t signature;

    // 0x28, SATA status (SCR0:SStatus)
    uint32_t sataStatus;
    // 0x2C, SATA control (SCR2:SControl)
    uint32_t sataControl;
    // 0x30, SATA error (SCR1: SError)
    uint32_t sataError;
    // 0x34, SATA active
    uint32_t sataActive;
    // 0x38, command issue
    uint32_t cmdIssue;
    // 0x3C, SATA notification (SCR4:SNotification)
    uint32_t sataNotification;

    // 0x40, FIS based switch control (what?)
    uint32_t fbs;

    uint32_t reserved1[11];
    // 0x70-0x7F: vendor specific
    uint32_t vendor[4];
} __attribute__((packed));
static_assert(sizeof(AhciHbaPortRegisters) == 0x80, "AhciHbaPortRegisters struct is wrong size");

/**
 * Bit fields for the AhciHbaPortRegisters `irqStatus` and `irqEnable` fields
 */
enum AhciPortIrqs: uint32_t {
    /// An error occurred while processing a task file
    TaskFileError                       = (1 << 30),
    /// HBA received more bytes than there are PRDs for
    ReceiveOverflow                     = (1 << 24),
    /// Port connectivity state changed
    PortStateChanged                    = (1 << 6),
    /// Descriptor processed interrupt
    DescriptorProcessed                 = (1 << 5),
    /// DMA setup FIS
    DmaSetup                            = (1 << 2),
    /// PIO setup FIS
    PioSetup                            = (1 << 1),
    /// Device to host register transfer
    DeviceToHostReg                     = (1 << 0),
};

/**
 * Bit fields for the AhciHbaPortRegisters `command` register
 */
enum AhciPortCommand: uint32_t {
    /// Command engine for this port is running
    CommandEngineRunning                = (1 << 15),
    /// FIS receive is in progress
    ReceiveFISRunning                   = (1 << 14),

    /// Enable FIS reception
    ReceiveFIS                          = (1 << 4),
    /// Enable command processing
    SendCommand                         = (1 << 0),
};

/**
 * Structure representing the register layout of an AHCI HBA's ABAR area.
 */
struct AhciHbaRegisters {
    // 0x00, Host capability
    uint32_t hostCaps;
    // 0x04, Global host control
    uint32_t ghc;
    // 0x08, Interrupt status
    uint32_t irqStatus;
    // 0x0C, Ports implemented 
    uint32_t portsImplemented;
    // 0x10, Version
    uint32_t version;
    // 0x14, Command completion coalescing control 
    uint32_t ccc_ctl;
    // 0x18, Command completion coalescing ports
    uint32_t ccc_pts;
    // 0x1C, Enclosure management location
    uint32_t em_loc;
    // 0x20, Enclosure management control
    uint32_t em_ctl;
    // 0x24, Host capabilities extended
    uint32_t hostCapsExt;
    // 0x28, BIOS/OS handoff control and status
    uint32_t bohc;

    // 0x2C - 0x9F, Reserved
    uint8_t reserved[0xA0-0x2C];

    // 0xA0 - 0xFF, Vendor specific registers
    uint8_t vendor[0x100-0xA0];

    // Up to 32 port specific register banks; check the `Ports Implemented` register
    AhciHbaPortRegisters ports[];
} __attribute__((packed));

/// Bit flags within the global host control (GHC) field
enum AhciGhc: uint32_t {
    /// When set, the controller operates in AHCI rather than legacy mode
    AhciEnable                          = (1U << 31),
    /// Indicates that the HBA is using irq sharing, because all MSIs couldn't be allocated.
    MsiSingleMessage                    = (1U << 2),
    /// Interrupts are enabled from HBA when set
    IrqEnable                           = (1U << 1),
    /// Write as 1 to reset HBA; reads 1 until reset completes
    Reset                               = (1U << 0),
};
/// Bit flags for the host capabilities field
enum AhciHostCaps: uint32_t {
    /// 64-bit addressing for various in memory structures is supported
    Supports64Bit                       = (1U << 31),
    /// The HBA supports SATA native command queuing and handles DMA setup FISes
    SataNCQ                             = (1U << 30),
    /// SATA Notifications (via SNotification) register are supported
    SNotification                       = (1U << 29),
    /// Devices can be spun up individually
    StaggeredSpinup                     = (1U << 28),
    /// Offset for the maximum supported HBA speed value
    HbaMaxSpeedOffset                   = 20,
    /// Bitmask for the maximum supported HBA speed
    HbaMaxSpeedMask                     = (0b1111 << HbaMaxSpeedOffset),
    /// The HBA supports port multipliers.
    PortMultipliers                     = (1U << 17),
    /// Offset for the 0 based "number of command slots" value
    NumCommandSlotsOffset               = 8,
    /// Mask for the 0 based "number of command slots" value
    NumCommandSlotsMask                 = (0b11111 << NumCommandSlotsOffset),
};
/// Bit flags for the extended capabilities (hostCapsExt) field
enum AhciHostCaps2: uint32_t {
    /// The HBA implements BIOS/OS handoff
    BiosHandoffSupported                = (1U << 0),
};

/// Bit flags for the BIOS/OS handoff control register
enum AhciBohc: uint32_t {
    /// Indicates that the BIOS is busy cleaning up
    BiosBusy                            = (1U << 4),
    /// Indicates the OS has requested ownership of the HBA
    OsOwnershipFlag                     = (1U << 1),
    /// Indicates BIOS owns the HBA
    BiosOwnershipFlag                   = (1U << 0),
};

#endif
