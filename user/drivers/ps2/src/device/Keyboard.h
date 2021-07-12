#ifndef DEVICE_KEYBOARD_H
#define DEVICE_KEYBOARD_H

#include "Ps2Device.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

#include <sys/bitflags.hpp>

enum class Scancode: uint32_t;

/// Flags for a key being processed.
enum class KeyFlags: uintptr_t {
    None                            = 0,

    /// Key code comes from the escaped set
    ScanCodeAlternate               = (1 << 0),
    /// The scan code indicates a "break" code
    Break                           = (1 << 7),
};
ENUM_FLAGS_EX(KeyFlags, uintptr_t);

/**
 * Implements a basic driver for a generic PS/2 keyboard.
 */
class Keyboard: public Ps2Device {
    /// Sets the scan code set in use by the keyboard.
    constexpr static const std::byte kCommandSetScanSet{0xF0};

    /// Escaped scan code (from first set) follows
    constexpr static const std::byte kScancodeEscape1{0xE0};
    /// Escaped scan code (from second set) follows
    constexpr static const std::byte kScancodeEscape2{0xE1};
    /// The subsequent scan code indicates a "break" event
    constexpr static const std::byte kScancodeBreak{0xF0};

    /// States for the scan code state machine
    enum class ScanState {
        Idle, ReadKey, ReadKeyOrBreak
    };

    public:
        Keyboard(Ps2Controller * _Nonnull controller, const Ps2Port port);
        ~Keyboard();

        /// Enables keyboard updates.
        void enableUpdates();
        /// Disables keyboard updates.
        void disableUpdates();

        /// Handles a received key code.
        void handleRx(const std::byte data) override;

    private:
        void handleScancode(const std::byte data);
        void generateKeyEvent(const std::byte key);

        static uint32_t ConvertScancode(const std::byte code, const KeyFlags flags);

    private:
        /// lookup table for the primary PS/2 scancode set
        static const std::unordered_map<std::byte, Scancode> kScancodePrimary;
        /// lookup table for the alternate PS/2 scancode set
        static const std::unordered_map<std::byte, Scancode> kScancodeAlternate;

        /// whether keyboard scanning is enabled
        std::atomic_bool enabled{false};

        /// state of the scancode inscretion machine
        ScanState scanState{ScanState::Idle};
        /// flags for the key event to generate next
        KeyFlags keyFlags{KeyFlags::None};
};

#endif
