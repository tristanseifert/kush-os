#ifndef DEVICE_GENERICMOUSE_H
#define DEVICE_GENERICMOUSE_H

#include "Ps2Device.h"

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include <sys/bitflags.hpp>

/// Flags in a PS/2 mouse packet
enum class MouseFlags: uint8_t {
    /// Y axis overflow
    YAxisOverflow                       = (1 << 7),
    /// X axis overflow
    XAxisOverflow                       = (1 << 6),
    /// Y axis sign bit
    YAxisSign                           = (1 << 5),
    /// X axis sign bit
    XAxisSign                           = (1 << 4),

    /// Left button
    Button1                             = (1 << 0),
    /// Middle button
    Button2                             = (1 << 2),
    /// Right button
    Button3                             = (1 << 1),
};
ENUM_FLAGS_EX(MouseFlags, uint8_t);



/**
 * Driver for a basic PS/2 mouse. The sample rate and resolution are fixed, as mouse cursor
 * scaling is to be implemented in higher levels for more fine grained control.
 *
 * Currently, only the standard dual axis, three button mouse is supported. This class is set up
 * such that a wheel mouse or five button mouse can easily be supported by creating a subclass that
 * simply does the required sequence of rate set calls to put the mouse into the advanced four
 * byte packet mode.
 */
class GenericMouse: public Ps2Device {
    protected:
        /// Sets the resolution used by the mouse, followed by one byte 0-3
        constexpr static const std::byte kCommandSetResolution{0xE8};
        /// Sets the sample rate; argument may be 10, 20, 40, 60, 80, 100 or 200
        constexpr static const std::byte kCommandSetSampleRate{0xF3};

        /// maximum mouse packet length (4 bytes with Z axis/5 button, 3 bytes otherwise)
        constexpr static const size_t kMaxPacketLen = 4;

        /**
         * Basic mouse data packet for three buttons and two axes. Note that the data is laid out
         * a little weirdly, as the 9th bit of the X/Y coordinates is in the flag field.
         */
        struct Packet {
            MouseFlags flags;
            // low 8 bits of the X axis movement
            uint8_t xm;
            // low 8 bits of Y axis movement
            uint8_t ym;

            public:
                /// Return the signed X axis movement
                constexpr inline int16_t getDx() {
                    return this->xm - (TestFlags(this->flags & MouseFlags::XAxisSign) ? 0x100 : 0);
                }
                /// Return the signed Y axis movement
                constexpr inline int16_t getDy() {
                    return this->ym - (TestFlags(this->flags & MouseFlags::YAxisSign) ? 0x100 : 0);
                }

                /// Whether a particular button is down
                constexpr inline bool isButtonDown(const size_t i) {
                    switch(i) {
                        case 0:
                            return TestFlags(this->flags & MouseFlags::Button1);
                        case 1:
                            return TestFlags(this->flags & MouseFlags::Button2);
                        case 2:
                            return TestFlags(this->flags & MouseFlags::Button3);
                        default:
                            return false;
                    }
                }
        } __attribute__((__packed__));

    public:
        GenericMouse(Ps2Controller * _Nonnull controller, const Ps2Port port);
        ~GenericMouse();

        /// Enables position updates.
        void enableUpdates();
        /// Disables position updates.
        void disableUpdates();

        /// Handles a received mouse data byte.
        void handleRx(const std::byte data) override;

    protected:
        /// Decodes a mouse packet
        virtual void handlePacket(const std::span<std::byte> &packet);

    private:
        /// Whether mouse input is enabled
        std::atomic_bool enabled{false};
        /// whether we've just been enabled
        bool freshlyEnabled{false};

        /// current write offset into packet buffer
        size_t packetBufOff{0};
        /// size of a complete packet
        size_t packetLen{3};
        /// receive buffer for mouse data packet
        std::array<std::byte, kMaxPacketLen> packetBuf;
};

#endif
