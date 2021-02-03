#ifndef PLATFORM_PC_ACPI_MANAGER_H
#define PLATFORM_PC_ACPI_MANAGER_H

#include <platform.h>

#include <stddef.h>
#include <stdint.h>

extern "C" void multiboot_parse();

struct multiboot_tag_old_acpi;

namespace platform { namespace acpi {
/**
 * A very basic ACPI table parser to get information on interrupt configuration (using the IO and
 * LAPICs) 
 */
class Manager {
    friend void ::multiboot_parse();
    friend void ::platform_vm_available();

    public:

    private:
        // RSDP v1 struct from bootloader
        struct RSDPv1 {
            // always 'RSD PTR '
            char signature[8];
            // sum all bytes in this struct and we should get 0
            uint8_t checksum;
            // a string OEM ID
            char OEMID[6];
            // ACPI version: 0 = ACPI 1.0, 2 = ACPI 2 and later
            uint8_t revision;
            // 32-bit physical address of the RSDT
            uint32_t rsdtPhysAddr;
        } __attribute__ ((packed));

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
                uint32_t sum = 0;
                for(size_t i = 0; i < this->length; i++) {
                    sum += ((const uint8_t *) this)[i];
                }
                return (sum & 0xFF) == 0;
            }
        } __attribute__ ((packed));

        /// 32-bit root system description table
        struct RSDT {
            struct SdtHeader head;
            uint32_t ptrs[0];

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
        static void init(struct multiboot_tag_old_acpi *info);
        static void vmAvailable() {
            gShared->parseTables();
        }

        Manager(const uint64_t _rsdtPhys);

        void parseTables();

        void parse(const MADT *);
        void madtRecord(const MADT::LocalApic *);
        void madtRecord(const MADT::IoApic *);
        void madtRecord(const MADT::IrqSourceOverride *);
        void madtRecord(const MADT::Nmi *);

        void parse(const HPET *);

    private:
        static Manager *gShared;

    private:
        /// Physical address of the RSDT
        uint64_t rsdtPhys = 0;
        /// RSDT as mapped into virtual memory
        RSDT *rsdt = nullptr;
};
}}

#endif
