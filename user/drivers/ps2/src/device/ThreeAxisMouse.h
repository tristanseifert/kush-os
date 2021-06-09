#ifndef DEVICE_THREEAXISMOUSE_H
#define DEVICE_THREEAXISMOUSE_H

#include "Ps2Command.h"
#include "GenericMouse.h"

#include <cstddef>
#include <cstdint>
#include <span>

/**
 * Implements the behavior changes needed to receive data for the third Z axis, which is provided
 * by the wheel of the mouse.
 */
class ThreeAxisMouse: public GenericMouse {
    /// Identification byte for the three axis mouse
    constexpr static const std::byte kIdentValue{0x03};

    public:
        /// Creates a three axis mouse from the given generic mouse.
        ThreeAxisMouse(const GenericMouse &from);
        ThreeAxisMouse() = delete;

        /// Attempts to upgrade a generic PS/2 mouse to Z-axis mode.
        static void TryUpgrade(GenericMouse *mouse);

    protected:
        /// Decodes a mouse packet
        void handlePacket(const std::span<std::byte> &packet) override;

    private:
        struct ZPacket: public GenericMouse::Packet {
            int8_t zm;

            /// Return the change in the Z acis
            constexpr inline int16_t getDz() {
                return this->zm;
            }
        } __attribute__((__packed__));

    private:
        static void UpgradeStep1(const Ps2Port p, Ps2Command *cmd, void *ctx);
        static void UpgradeStep2(const Ps2Port p, Ps2Command *cmd, void *ctx);
        static void UpgradeStep3(const Ps2Port p, Ps2Command *cmd, void *ctx);
        static void UpgradeIdentReply(const Ps2Port p, Ps2Command *cmd, void *ctx);
};

#endif
