#ifndef AHCIDRV_PORTSTRUCTS_H
#define AHCIDRV_PORTSTRUCTS_H

#include <cstdint>

/**
 * Defines the known FIS types.
 */
enum FISType: uint8_t {
    /// Host to device: register transfer
    RegisterHostToDevice                = 0x27,
    /// Device to host: register transfer
    RegisterDeviceToHost                = 0x34,
    /// Device to host: DMA activate
    DMAActivate                         = 0x39,
    ///  Bidirectional: DMA setup
    DMASetup                            = 0x41,
    ///  Bidirectional: Data transfer
    Data                                = 0x46,
    ///  Bidirectional: Built-in self test
    BIST                                = 0x58,
    /// Device to host: PIO transfer setup
    PIOSetup                            = 0x5F,
    /// Device to host: Set device bits
    DeviceBits                          = 0xA1,
};

/**
 * Host to device register FIS; this is used by the host to send a command or some form of control
 * to the device.
 */
struct RegHostToDevFIS {
    FISType type{FISType::RegisterHostToDevice};

    /// Port multiplier flag
    uint8_t pmport:4{0};
    uint8_t reserved0:3{0};
    /// Set if this is a command, 0 for control
    uint8_t c:1;

    /// Command register
    uint8_t command{0};
    /// Low 8 bits of feature register
    uint8_t featurel{0};

    /// Bits 0:7 of LBA register
    uint8_t lba0{0};
    /// Bits 8:15 of LBA register
    uint8_t lba1{0};
    /// Bits 16:23 of LBA register
    uint8_t lba2{0};
    /// Device register; should usually be always 0 for master
    uint8_t device{0};

    /// Bits 24:31 of LBA register
    uint8_t lba3{0};
    /// Bits 32:39 of LBA register
    uint8_t lba4{0};
    /// Bits 40:47 of LBA register
    uint8_t lba5{0};
    /// High 8 bits of the feature register
    uint8_t featureh{0};

    /// Low 8 bits of count register
    uint8_t countl{0};
    /// High 8 bits of count register
    uint8_t counth{0};
    /// Isynchronous command completion flag
    uint8_t icc{0};
    /// Control register
    uint8_t control{0};

    /// Reserved; initialize to 0
    uint8_t reserved1[4] {0, 0, 0, 0};
} __attribute__((packed));

/**
 * Device to host register FIS; this is sent by devices to notify the host that some ATA registers
 * have changed.
 */
struct RegDevToHostFIS {
    FISType type;
 
    /// Port multiplier flag
    uint8_t pmport:4;
    uint8_t reserved0:2;
    /// When set, the device is signaling an interrupt
    uint8_t i:1;
    uint8_t reserved1:1;

    /// Status register
    uint8_t status;
    /// Error register
    uint8_t error;

    /// Bits 0:7 of LBA register
    uint8_t lba0;
    /// Bits 8:15 of LBA register
    uint8_t lba1;
    /// Bits 16:23 of LBA register
    uint8_t lba2;
    /// Device register
    uint8_t device;

    /// Bits 24:31 of LBA register
    uint8_t lba3;
    /// Bits 32:39 of LBA register
    uint8_t lba4;
    /// Bits 40:47 of LBA register
    uint8_t lba5;
    uint8_t reserved2;

    /// Low 8 bits of count register
    uint8_t countl;
    /// High 8 bits of count register
    uint8_t counth;

    uint8_t reserved3[6];
} __attribute__((packed));

/**
 * Device to host set device bits FIS: Updates the "shadow register" component of the status and
 * error registers.
 *
 * TODO: Figure out the actual structure of this FIS for SATA 3... ignore it for now
 */
struct DeviceBitFIS {
    FISType type;

    /// Port multiplier flag
    uint8_t  pmport:4;
    uint8_t  rsv0:4;

    uint8_t stuff[6];
} __attribute__((packed));

/**
 * Bidirectional data FIS; used to send actual payloads of commands
 */
struct DataFIS {
    FISType type;

    /// Port multiplier flag
    uint8_t pmport:4;
    uint8_t reserved0:4;

    uint8_t reserved1[2];

    /// data payload, in 4 byte increments
    uint32_t data[];
} __attribute__((packed));

/**
 * PIO setup FIS; prepares a device to host programmed data transfer.
 */
struct PioSetupFIS {
    FISType type;

    /// Port multiplier flag
    uint8_t pmport:4;
    uint8_t reserved0:1;
    /// Direction of the PIO transfer; 1 = device to host, 0 = host to device
    uint8_t direction:1;
    /// Interrupt bit	
    uint8_t i:1;
    uint8_t reserved1:1;

    /// Status register
    uint8_t status;
    /// Error register
    uint8_t error;

    /// Bits 0:7 of LBA register
    uint8_t lba0;
    /// Bits 8:15 of LBA register
    uint8_t lba1;
    /// Bits 16:23 of LBA register
    uint8_t lba2;
    /// Device register
    uint8_t device;

    /// Bits 24:31 of LBA register
    uint8_t lba3;
    /// Bits 32:39 of LBA register
    uint8_t lba4;
    /// Bits 40:47 of LBA register
    uint8_t lba5;
    uint8_t reserved2;

    /// Low 8 bits of count register
    uint8_t countl;
    /// High 8 bits of count register
    uint8_t counth;
    uint8_t reserved3;
    /// New value of the status register
    uint8_t newStatus;

    /// Transfer count
    uint16_t tc;
    uint8_t reserved4[2];
} __attribute__((packed));

/**
 * DMA setup FIS; prepares a device to host transfer.
 */
struct DmaSetupFIS {
    FISType type;

    /// Port multiplier flag
    uint8_t pmport:4;
    uint8_t reserved0:1;
    /// Direction of the DMA transfer; 1 = device to host, 0 = host to device
    uint8_t direction:1;
    /// Interrupt bit	
    uint8_t i:1;
    /// Auto activate: if set, the "DMA Activate" FIS does not need to be sent
    uint8_t a:1;

    uint8_t reserved1[2];

    /// Host specific DMA buffer identifier
    uint64_t dmaBufferId;

    uint8_t reserved2[4];

    /// Byte offset into the DMA buffer; must be 4 byte aligned
    uint32_t dmaBufferOffset;
    /// Number of bytes to transfer; must be a multiple of 2 bytes
    uint32_t dmaTransferCount;

    uint8_t reserved3[4];
} __attribute__((packed));


/**
 * Received FIS structure for a port; this must be 256 byte aligned.
 */
struct PortReceivedFIS {
    /// DMA setup FIS
    DmaSetupFIS dsfis;
    uint8_t reserved0[4];

    /// PIO setup FIS
    PioSetupFIS psfis;
    uint8_t reserved1[12];

    /// Register device to host FIS
    RegDevToHostFIS rfis;
    uint8_t reserved2[4];

    /// Set device bit FIS
    DeviceBitFIS sdbfis;

    /// Unknown/unsupported FIS (up to 64 bytes)
    uint8_t ufis[64];

    // Reserved
    uint8_t reserved3[0x100-0xA0];
} __attribute__((packed));

static_assert(sizeof(PortReceivedFIS) == 0x100, "Invalid size for PortReceivedFIS");
static_assert(offsetof(PortReceivedFIS, dsfis) == 0x00, "DMA setup FIS offset wrong");
static_assert(offsetof(PortReceivedFIS, psfis) == 0x20, "PIO setup FIS offset wrong");
static_assert(offsetof(PortReceivedFIS, rfis) == 0x40, "Device-to-host register FIS offset wrong");
static_assert(offsetof(PortReceivedFIS, sdbfis) == 0x58, "Set device bits FIS offset wrong");
static_assert(offsetof(PortReceivedFIS, ufis) == 0x60, "Unknown FIS offset wrong");


/**
 * Defines a single command header that exists inside the command list.
 */
struct CommandHeader {
    /// Length of the command FIS, in 4 byte units
    uint8_t commandFisLen:5;
    /// When set, the command is ATAPI
    uint8_t atapi:1;
    /// Write: 1 = host to device, 0 = device to host
    uint8_t write:1;
    /// Prefetchable
    uint8_t prefetchable:1;

    uint8_t reset:1;
    uint8_t bist:1;
    /// Clear the busy flag when the command is sent with an R_OK response
    uint8_t clearBusy:1;
    uint8_t reserved0:1;
    /// Port multiplier flag
    uint8_t pmport:4;

    /// Length of the physical region descriptor table, in entries
    uint16_t prdEntries;

    /// Number of physical region descriptor bytes transfered
    uint32_t prdByteCount;

    /// Low 32 bits of command table descriptor base physical address
    uint32_t cmdTableBaseLow;
    /// High 32 bits of the command table descriptor base physical address
    uint32_t cmdTableBaseHigh;

    uint32_t reserved1[4];
} __attribute__((packed));

/**
 * Port command list; contains up to 32 slots for commands to be sent. Less than the full number of
 * slots may be supported by the HBA, however; the host capability register will indicate this.
 *
 * This structure must be allocated on an 1K boundary.
 */
struct PortCommandList {
    CommandHeader commands[32];
} __attribute__((packed));

static_assert(sizeof(PortCommandList) == 0x400, "Invalid size for PortCommandList");

/**
 * Defines a single contiguous physical memory region from/to which data is transfered.
 */
struct PortCommandTablePrd {
    /// Low 32 bits of physical region address
    uint32_t physAddrLow;
    /// High 32 bits of physical region address
    uint32_t physAddrHigh;

    uint32_t reserved0;

    /// When set, generate an interrupt when the transfer completes
    uint32_t irqOnCompletion:1;
    uint32_t reserved1:9;
    /// Total number of bytes to transfer for this descriptor
    uint32_t numBytes:22;
} __attribute__((packed));
static_assert(sizeof(PortCommandTablePrd) == 0x10, "Invalid size for PortCommandTablePrd");

/**
 * Port command table; defines a single command to be sent to the SATA device (either ATA or ATAPI)
 * and the physical region descriptors that describe the payload/data transfer to occur alongside
 * the command.
 */
struct PortCommandTable {
    /// Command FIS, up to 64 bytes
    uint8_t commandFIS[64];
    /// ATAPI command, 12 or 16 bytes
    uint8_t atapiCommand[16];

    uint8_t reserved0[48];

    /// Optional physical region descriptors (memory regions for payload data)
    PortCommandTablePrd descriptors[];
} __attribute__((packed));

static_assert(sizeof(PortCommandTable) == 0x80, "Invalid size for PortCommandTable");

#endif
