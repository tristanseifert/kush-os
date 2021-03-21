#ifndef PLATFORM_PC64_ACPI_PARSER_H
#define PLATFORM_PC64_ACPI_PARSER_H

#include <stddef.h>
#include <stdint.h>

namespace platform {
/**
 * Parses ACPI tables (based on the location discovered through the BOOTBOOT information structure)
 * to discover interrupt configuration.
 */
class AcpiParser {
    friend class IrqManager;
    friend class LocalApic;
    friend class IoApic;
    friend class Hpet;

    public:
        /// Parses tables; should be called on BSP only
        static void Init();
        /// Return shared ACPI parser
        static inline AcpiParser *the() {
            return gShared;
        }

    private:
        /// Base address of the physical memory identity mapping zone
        constexpr static uintptr_t kPhysIdentityMap = 0xffff800000000000;

        /// Header of an ACPI system description table
        struct SdtHeader {
            /// Table signature
            char signature[4];
            /// Total size of this table, INCLUDING header
            uint32_t length;
            /// table version
            uint8_t revision;
            /// checksum value (when added together should result in LSB = 0x00)
            uint8_t checksum;

            /// OEM ID for the table
            char OEMID[6];
            /// Table OEM ID
            char OEMTableID[8];
            /// OEM data revision
            uint32_t OEMRevision;
            /// Creator ID
            uint32_t creatorID;
            /// Creator revision
            uint32_t creatorRevision;

            /// Verify checksum
            bool validateChecksum() const {
                uintptr_t sum = 0;
                for(size_t i = 0; i < this->length; i++) {
                    sum += ((const uint8_t *) this)[i];
                }
                return (sum & 0xFF) == 0;
            }
        } __attribute__ ((packed));

        /// 64-bit root system description table
        struct XSDT {
            struct SdtHeader head;
            uintptr_t ptrs[0];

            /// verify the checksum of the entire RSDT
            bool validateChecksum() const {
                return this->head.validateChecksum();
            }
        } __attribute__ ((packed));

        /// APIC description table
        struct MADT {
            /// Generic header shared by all MADT records
            struct RecordHdr {
                /// type of record
                uint8_t type;
                /// length of this record including header
                uint8_t length;
            } __attribute__ ((packed));

            /// Type 0: processor local APIC structure
            struct LocalApic {
                enum Flags: uint32_t {
                    /// The CPU is enabled
                    PROC_ENABLED                = (1 << 0),
                    /// The processor may be onlined by the OS
                    PROC_ONLINE_CAPABLE         = (1 << 1),
                };

                /// MADT record header
                RecordHdr hdr;

                /// Processor ID
                uint8_t cpuId;
                /// ID of the corresponding APIC
                uint8_t apicId;
                /// more info about this APIC mapping
                Flags flags;
            } __attribute__ ((packed));
            /// Type 1: Global IO APIC definition
            struct IoApic {
                RecordHdr hdr;

                /// ID of the IO APIC
                uint8_t apicId;
                /// reserved field, should be 0
                uint8_t reserved;
                /// MMIO address of the IO APIC
                uint32_t ioApicPhysAddr;
                /// Global system interrupt base
                uint32_t irqBase;
            } __attribute__ ((packed));
            /// Type 2: Interrupt source override
            struct IrqSourceOverride {
                /// MADT record heade
                enum Flags: uint16_t {
                    /// Active low when set
                    ACTIVE_LOW                  = (1 << 1),
                    /// Level triggered if set
                    TRIG_LEVEL                  = (1 << 3),
                };

                /// MADT record header
                RecordHdr hdr;

                uint8_t busSource;
                uint8_t irqSource;
                uint32_t systemIrq;
                Flags flags;
            } __attribute__ ((packed));
            /// Type 4: NMI config
            struct Nmi {
                enum Flags: uint16_t {
                    /// Active low when set
                    ACTIVE_LOW                  = (1 << 1),
                    /// Level triggered if set
                    TRIG_LEVEL                  = (1 << 3),
                };

                /// MADT record header
                RecordHdr hdr;

                /// processors for which this is the NMI vector (0xFF means all)
                uint8_t cpuId;
                /// flags
                Flags flags;
                /// local interrupt number (goes into the processor LAPIC LINTx regs)
                uint8_t lint;
            } __attribute__ ((packed));

            /// table header
            struct SdtHeader head;

            /// 32-bit address of the local APIC
            uint32_t lapicAddr;
            /// flags; if bit 0 is set, legacy PICs need to be disabled
            uint32_t flags;

            // records
            RecordHdr records[];
        } __attribute__ ((packed));

        // Event timer (HPET) description table
        struct HPET {
            /// HPET address info
            struct AddressInfo {
                enum class AddressSpace: uint8_t {
                    MMIO                = 0,
                    LegacyIO            = 1,
                };

                /// Address space for the HPET
                AddressSpace spaceId;
                /// Register bit width
                uint8_t regWidth;
                /// Register bit offset
                uint8_t regOffset;
                uint8_t reserved;

                /// Physical address
                uint64_t physAddr;
            } __attribute__ ((packed));

            /// table header
            struct SdtHeader head;

            /// HPET hardware revision
            uint8_t hwRev;
            /// Number of comparators
            uint8_t numComparators      : 5;
            /// When set, comparators are 64-bit wide
            uint8_t counter64           : 1;
            uint8_t reserved            : 1;
            /// ???
            uint8_t legacyReplace       : 1;

            uint16_t pci_vendor_id;

            struct AddressInfo address;

            /// HPET number
            uint8_t hpetNo;
            /// Minimum tick period
            uint16_t minTick;
            uint8_t pageProtection;
        } __attribute__ ((packed));

    private:
        /// Creates an ACPI parser with a given RSDT physical address
        AcpiParser(const uintptr_t rsdtPhys);

        /// An ACPI table was discovered at the given physical address.
        void foundTable(const uintptr_t phys);

        /// Parse a MADT (APIC description) table
        void parse(const MADT *);
        void madtRecord(const MADT *, const MADT::LocalApic *);
        void madtRecord(const MADT *, const MADT::IoApic *);
        void madtRecord(const MADT *, const MADT::IrqSourceOverride *);
        void madtRecord(const MADT *, const MADT::Nmi *);
        /// Parse a HPET table
        void parse(const HPET *);

    private:
        /// global ACPI table parser
        static AcpiParser *gShared;

        /// whether discovered tables are logged
        static bool gLogTables;
        /// whether discovered LAPIC info is logged
        static bool gLogLapic;
        /// whether discovered IOAPIC info is logged
        static bool gLogIoapic;
        /// whether interrupt routings are logged
        static bool gLogApicRoutes;
        /// whether found HPET tables are logged
        static bool gLogHpet;

    private:
        /// Physical base address
        uintptr_t rsdpPhys = 0;

        /// Location of the MADT
        const MADT *apicInfo = nullptr;
        /// Location of the first HPET table
        const HPET *hpetInfo = nullptr;
};
};

#endif
