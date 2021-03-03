#ifndef BUS_PCICONFIG_H
#define BUS_PCICONFIG_H

#include <cassert>

/**
 * Provides an interface to accessing PCI configuration registers.
 */
class PciConfig {
    public:
        enum class Width {
            Byte                        = 8,
            Word                        = 16,
            QWord                       = 32,
        };

    public:
        /// Initialize the PCI config instance
        static void init() {
            assert(!gShared);
            gShared = new PciConfig;
        }

        /// Return the global PCI config instance
        static PciConfig *the() {
            return gShared;
        }

        /// Reads a PCI config register.
        const uint32_t read(const uint8_t bus, const uint8_t device, const uint8_t func,
                const size_t reg, const Width width);
        /// Writes a PCI config register.
        const void write(const uint8_t bus, const uint8_t device, const uint8_t func,
                const size_t reg, const Width width, const uint32_t value);

    private:
        PciConfig();

    private:
        static PciConfig *gShared;
};

#endif
